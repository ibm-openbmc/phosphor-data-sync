// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <sys/inotify.h>
#include <unistd.h>

#include <sdbusplus/async.hpp>

#include <filesystem>
#include <map>
#include <mutex>

namespace data_sync::watch::inotify
{

namespace fs = std::filesystem;

/**
 * @brief A tuple which has the info related to the occured inotify event
 *
 * int         - Watch descriptor corresponds to the event
 * std::string - name[] in inotify_event struct
 * uint32_t    - Mask describing event
 */
using WD = int;
using BaseName = std::string;
using EventMask = uint32_t;
using Cookie = uint32_t;
using EventInfo = std::tuple<WD, BaseName, EventMask, Cookie>;

/**
 * @brief enum which indicates the type of operations that can take against an
 * intersted inotify event on a configured data path
 */
enum class DataOps
{
    COPY,
    DELETE
};

/**
 * @brief Container holding data paths and their corresponding operations.
 *
 * Represents a list of data paths and the type of operation,that needs to be
 * performed on each path.
 *
 * Each entry is a pair of :
 * - fs::path : Absolute path of the file or directory
 * - DataOps  : Operation to perform on the given path
 */
using DataOperation = std::pair<fs::path, DataOps>;
using DataOperations = std::vector<DataOperation>;

/** @class FD
 *
 *  @brief RAII wrapper for file descriptor.
 */
class FD
{
  public:
    FD(const FD&) = delete;
    FD& operator=(const FD&) = delete;
    FD(FD&&) = delete;
    FD& operator=(FD&&) = delete;

    /** @brief Saves File descriptor and uses it to do file operation
     *
     *  @param[in] fd - File descriptor
     */
    explicit FD(int fd) : fd(fd) {}

    ~FD()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }

    int operator()() const
    {
        return fd;
    }

  private:
    /** @brief File descriptor */
    int fd = -1;
};

/** @class DataWatcher
 *
 *  @brief Adds inotify watch on directories/files configured for sync.
 *
 */
class DataWatcher
{
  public:
    DataWatcher(const DataWatcher&) = delete;
    DataWatcher& operator=(const DataWatcher&) = delete;
    DataWatcher(DataWatcher&&) = delete;
    DataWatcher& operator=(DataWatcher&&) = delete;

    /**
     * @brief Constructor
     *
     * Create watcher for directories/files to monitor for the occurence
     * of the interested events upon modifications.
     *
     *  @param[in] ctx - The async context object
     *  @param[in] inotifyFlags - inotify flags to watch
     *  @param[in] eventMasksToWatch - mask of interested events to watch
     *  @param[in] dataPathToWatch - Path of the file/directory to be watched
     */
    DataWatcher(sdbusplus::async::context& ctx, int inotifyFlags,
                uint32_t eventMasksToWatch,
                const std::filesystem::path& dataPathToWatch);

    /**
     * @brief Destructor
     * Remove the inotify watch and close fd's
     */
    ~DataWatcher();

    /**
     * @brief API to monitor for the file/directory for inotify events
     *
     * @returns bool - true - Inidcates that sync action is required on the
     *                        received event
     *                 false - Inidcates that no action is required on the
     *                         received event
     */
    sdbusplus::async::task<DataOperations> onDataChange();

    /**
     * @brief API to get all the instances of the DataWatcher
     *
     * @returns std::vector<DataWatcher*> - A reference to the vector containing
     * all instances of this class
     */
    static std::vector<DataWatcher*>& getAllWatchers()
    {
        static std::vector<DataWatcher*> instances;
        return instances;
    }

    /**
     * @brief API to get the reference to the static mutex to guard
     * concurrent access to shared data (i.e., the vector of DataWatcher
     * instances)
     *
     * @returns std::mutex& - A reference to the static mutex
     */
    static std::mutex& getMutex()
    {
        static std::mutex mtx;
        return mtx;
    }

    /**
     * @brief API to get the file descriptor of the inotify instance
     *
     * @returns std::map<WD, fs::path>& - A reference to the map of WD and their
     * associated file paths.
     */
    const std::map<WD, fs::path>& getWatchDescriptors() const
    {
        return _watchDescriptors;
    }

  private:
    /**
     * @brief inotify flags
     */
    int _inotifyFlags;

    /**
     * @brief The group of interested event Masks for which data to be watched
     */
    uint32_t _eventMasksToWatch;

    /**
     * @brief The group of events for which the data need to be watched on the
     * parent if the configured path is not exisitng in the filesystem.
     *
     * If the configured path is a file the _eventMasksToWatch will be
     * IN_CLOSE_WRITE | IN_DELETE_SELF. But if file not exists, we need to
     * monitor IN_CREATE | IN_DELETE also.
     */
    uint32_t _eventMasksIfNotExists = IN_CREATE | IN_CLOSE_WRITE | IN_DELETE |
                                      IN_DELETE_SELF;

    /**
     * @brief File/Directory path to be watched
     */
    std::filesystem::path _dataPathToWatch;

    /**
     * @brief The map of unique watch descriptors associated with an configured
     * file or directory.
     */
    std::map<WD, fs::path> _watchDescriptors;

    /**
     * @brief file descriptor referring to the inotify instance
     */
    FD _inotifyFileDescriptor;

    /**
     * @brief fdio instance
     */
    std::unique_ptr<sdbusplus::async::fdio> _fdioInstance;

    /**
     * @brief Map of DataOperation
     */
    DataOperations _dataOperations;

    /**
     * @brief Map of Cookie and DataOperation to save the inotify event info.
     *
     * Eg : The IN_MOVED_FROM info during the rename or move operations can be
     * saved for map to the corresponding IN_MOVED_TO signals.
     */
    std::map<Cookie, DataOperation> _movedFromDataOps;

    /**
     * @brief initialize an inotify instance and returns file descriptor
     */
    int inotifyInit() const;

    /**
     * @brief API to convert the inotify event masks to event macros in string
     *        format.
     *
     * @param[in] - eventMask - The mask describing event
     *
     * @returns - The event name in string format.
     */
    static std::string eventName(uint32_t eventMask);

    /**
     * @brief API to get the existing parent path of a given path.
     *
     * @param[in] dataPath - The path to check

     * @returns std::filesystem::path - existing parent path
     */
    static fs::path getExistingParentPath(const fs::path& dataPath);

    /**
     * @brief Create watcher for the given data path and add to the list of
     * watch descriptors.
     *
     * @param[in] pathToWatch - The path of file/directory to be monitored
     * @param[in] eventMasksToWatch - The set of events for which the path to be
     *                                monitored
     */
    void addToWatchList(const fs::path& pathToWatch,
                        uint32_t eventMasksToWatch);

    /** @brief API to create watchers for the given path and also for the
     * subdirectories if exists inside the configured directory path.
     *
     * @param[in] pathToWatch - The path of file/directory to be monitored.
     */
    void createWatchers(const std::filesystem::path& pathToWatch);

    /**
     * @brief API to read the triggered events from inotify structure
     *
     * returns : The vector of events read from the buffer
     *         : std::nullopt , in case of any errors while reading from buffer
     */
    std::optional<std::vector<EventInfo>> readEvents();

    /**
     * @brief API to trigger processing of the received inotify events.
     *
     * @param[in] receivedEvents : The vector of type eventInfo type which has
     *                             the information of received inotify events.
     *
     */
    void processEvents(const std::vector<EventInfo>& receivedEvents);

    /**
     * @brief API to process each of the received inotify event and to determine
     * the type of operation need to trigger for the event.
     *
     * @param[in] receivedEvent : eventInfo type which has the information of
     *                            received  inotify event.
     *
     * @returns DataOperation : If the received event need to handle in rsync
     *          std::nullopt  : If the received event doesn't need to handle.
     */
    std::optional<DataOperation> processEvent(const EventInfo& receivedEvent);

    /**
     * @brief API to handle the received IN_CLOSE_WRITE inotify events
     *
     * @param[in] receivedEventInfo : eventInfo type which has the information
     *                                of received  inotify event.
     *
     * @returns DataOperation : If the received event need to handle in rsync
     *          std::nullopt  : If the received event doesn't need to handle.
     */
    std::optional<DataOperation>
        processCloseWrite(const EventInfo& receivedEventInfo);

    /**
     * @brief API to handle the received IN_CREATE inotify events
     *
     * @param[in] receivedEventInfo : eventInfo type which has the information
     *                                of received  inotify event.
     *
     * @returns DataOperation : If the received event need to handle in rsync
     *          std::nullopt  : If the received event doesn't need to handle.
     */
    std::optional<DataOperation>
        processCreate(const EventInfo& receivedEventInfo);

    /**
     * @brief API to handle the received IN_MOVED_FROM inotify events
     *
     * @param[in] receivedEventInfo : eventInfo type which has the information
     *                                of received  inotify event.
     *
     * @returns DataOperation : If the received event need to handle in rsync
     *          std::nullopt  : If the received event doesn't need to handle.
     */
    std::optional<DataOperation>
        processMovedFrom(const EventInfo& receivedEventInfo);

    /**
     * @brief API to handle the received IN_MOVED_TO inotify events
     *
     * @param[in] receivedEventInfo : eventInfo type which has the information
     *                                of received  inotify event.
     *
     * @returns DataOperation : If the received event need to handle in rsync
     *          std::nullopt  : If the received event doesn't need to handle.
     */
    std::optional<DataOperation>
        processMovedTo(const EventInfo& receivedEventInfo);

    /**
     * @brief API to handle the received IN_DELETE inotify events
     *
     * @param[in] receivedEventInfo : eventInfo type which has the information
     *                                of received  inotify event.
     *
     * @returns DataOperation : If the received event need to handle in rsync
     *          std::nullopt  : If the received event doesn't need to handle.
     */
    std::optional<DataOperation>
        processDelete(const EventInfo& receivedEventInfo);

    /**
     * @brief API to handle the received IN_DELETE_SELF inotify events
     *
     * @param[in] receivedEventInfo : eventInfo type which has the information
     *                                of received  inotify event.
     *
     * @returns DataOperation : If the received event need to handle in rsync
     *          std::nullopt  : If the received event doesn't need to handle.
     */
    std::optional<DataOperation>
        processDeleteSelf(const EventInfo& receivedEventInfo);

    /** @brief API to remove the watch for a path and to
     *  remove from the map of watch descriptors.
     *
     *  @param[in] wd - Watch descriptor corresponding to the path
     */
    void removeWatch(int wd);
};

} // namespace data_sync::watch::inotify
