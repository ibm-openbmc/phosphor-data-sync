// SPDX-License-Identifier: Apache-2.0

#include "error_log.hpp"

#include <unistd.h>

#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>

#include <sstream>

namespace data_sync
{
namespace error_log
{

using namespace phosphor::logging;

FFDCFile::FFDCFile(FFDCFormat format, FFDCSubType subType, FFDCVersion version,
                   const std::string& data) :
    _format(format), _subType(subType), _version(version),
    _fileName("/tmp/syncDataFFDCFile.XXXXXX"), _fd(-1), _data(data)
{
    prepareFFDCFile();
}

FFDCFile::~FFDCFile()
{
    removeFFDCFile();
}

void FFDCFile::prepareFFDCFile()
{
    createFFDCFile();
    writeFFDCData();
    resetFFDCFileSeekPos();
}

void FFDCFile::createFFDCFile()
{
    _fd = mkstemp(_fileName.data());

    if (_fd == -1)
    {
        lg2::error("Failed to create FFDC file {FILE_NAME}", "FILE_NAME",
                   _fileName);
        throw std::runtime_error("Failed to create FFDC file");
    }
}

void FFDCFile::writeFFDCData()
{
    ssize_t rc = write(_fd, _data.data(), _data.size());

    if (rc == -1)
    {
        lg2::error("Failed to write any FFDC info in the file {FILE_NAME}",
                   "FILE_NAME", _fileName);
        throw std::runtime_error("Failed to write FFDC info");
    }
    else if (rc != static_cast<ssize_t>(_data.size()))
    {
        lg2::error("Failed to write all FFDC info in the file {FILE_NAME}",
                   "FILE_NAME", _fileName);
    }
}

void FFDCFile::resetFFDCFileSeekPos()
{
    if (lseek(_fd, 0, SEEK_SET) == (off_t)-1)
    {
        lg2::error("Failed to set SEEK_SET for FFDC file {FILE_NAME}",
                   "FILE_NAME", _fileName);
        throw std::runtime_error("Failed to set SEEK_SET for FFDC file");
    }
}

void FFDCFile::removeFFDCFile()
{
    close(_fd);
    std::remove(_fileName.data());
}

FFDCFileSet::FFDCFileSet(const bool collectTraces, const json& calloutData)
{
    if (collectTraces)
    {
        try
        {
            createFFDCFilesForTraces();
        }
        catch (const std::exception& e)
        {
            lg2::error("Exception while collecting traces for FFDC: {ERROR}",
                       "ERROR", e.what());
        }
    }

    if (!calloutData.is_null())
    {
        try
        {
            createFFDCFilesForCallouts(calloutData);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "Exception while collecting callout data for FFDC: {ERROR}",
                "ERROR", e.what());
        }
    }
}

std::optional<std::string>
    FFDCFileSet::getTraceFieldValue(sd_journal* journal,
                                    const std::string& fieldName)
{
    const void* raw_data{nullptr};
    size_t length{0};

    auto rc = sd_journal_get_data(journal, fieldName.c_str(), &raw_data,
                                  &length);
    if (0 == rc)
    {
        std::string fieldValue(static_cast<const char*>(raw_data), length);

        // The data returned by sd_journal_get_data will be prefixed with the
        // field name and "=".
        if (auto fieldValPos = fieldValue.find('=');
            fieldValPos != std::string::npos)
        {
            // Just return field value
            return fieldValue.substr(fieldValPos + 1);
        }
        else
        {
            // Should not happen as per sd_journal_get_data documentation
            lg2::error(
                "Failed to find the journal field separator [=] in the retrieved field {FIELD_VALUE}",
                "FIELD_VALUE", fieldValue);
        }
    }
    else
    {
        lg2::error(
            "Failed to get the given journal  {FIELD_NAME}, errorno: {ERROR_NO}, errormsg: {ERROR_MSG}",
            "FIELD_NAME", fieldName, "ERROR_NO", rc, "ERROR_MSG", strerror(rc));
    }
    return std::nullopt;
}

std::optional<std::vector<std::string>>
    FFDCFileSet::getJournalTraces(const std::string& fieldValue,
                                  const unsigned int maxTraces)
{
    // systemd journal context
    sd_journal* journal;
    int ret = sd_journal_open(&journal, SD_JOURNAL_LOCAL_ONLY);
    if (0 == ret)
    {
        std::vector<std::string> traces;

        // To get the latest entries, iterating the journal in reverse order
        SD_JOURNAL_FOREACH_BACKWARDS(journal)
        {
            auto retValue = getTraceFieldValue(journal, "SYSLOG_IDENTIFIER");
            // Get the next traces if not the fieldValue
            if (!retValue.has_value() || *retValue != fieldValue)
            {
                continue;
            }

            // Get timeStamp
            std::string timeStamp;
            uint64_t usec{0};
            if (0 == sd_journal_get_realtime_usec(journal, &usec))
            {
                // Converting realtime microseconds to human readable format
                // E.g. Nov 06 2025 14:23:45
                std::time_t timeInSec = static_cast<time_t>(usec / 1000000);
                std::stringstream ss;
                ss << std::put_time(std::localtime(&timeInSec),
                                    "%b %d %Y %H:%M:%S");
                timeStamp = ss.str();
            }

            // Get Process PID
            std::string pid;
            retValue = getTraceFieldValue(journal, "_PID");
            if (retValue.has_value())
            {
                pid = *retValue;
            }

            // Get Message
            std::string message;
            retValue = getTraceFieldValue(journal, "MESSAGE");
            if (retValue.has_value())
            {
                message = *retValue;
            }

            // Format -> Timestamp : ProcessName[ProcessPID] : Message
            std::stringstream trace;
            trace << timeStamp << " : " << fieldValue << "[" << pid
                  << "] : " << message;
            traces.push_back(trace.str());

            if (traces.size() == maxTraces)
            {
                break;
            }
        }

        sd_journal_close(journal);

        if (!traces.empty())
        {
            return traces;
        }
        else
        {
            lg2::info("No journal traces found for {FIELD_VALUE}",
                      "FIELD_VALUE", fieldValue);
        }
    }
    else
    {
        lg2::error(
            "Failed to get systemd journal traces for {FIELD_VALUE}, error code: {ERROR_CODE}, error message: {ERROR_MSG}",
            "FIELD_VALUE", fieldValue, "ERROR", ret, "ERROR_MSG",
            strerror(ret));
    }
    return std::nullopt;
}

void FFDCFileSet::createFFDCFilesForTraces()
{
    std::vector<std::string> apps{"phosphor-rbmc-data-sync-mgr", "stunnel"};
    for (const auto& app : apps)
    {
        auto traces = getJournalTraces(app);
        if (traces.has_value() && !traces->empty())
        {
            std::string traceData;
            for (const auto& line : *traces)
            {
                traceData.append(line);
                if (!line.ends_with('\n'))
                {
                    traceData.append("\n");
                }
            }
            // For traces FFDCFomat::Text - FFDC SubType "0", FFDC Version "0"
            _ffdcFiles.emplace_back(
                std::make_unique<FFDCFile>(FFDCFormat::Text, 0, 0, traceData));
        }
    }
}

void FFDCFileSet::createFFDCFilesForCallouts(const json& calloutData)
{
    // For callouts - FFDC SubType "0xCA", FFDC Version "0x01"
    _ffdcFiles.emplace_back(std::make_unique<FFDCFile>(
        FFDCFormat::JSON, 0xCA, 0x01, calloutData.dump()));
}

} // namespace error_log
} // namespace data_sync
