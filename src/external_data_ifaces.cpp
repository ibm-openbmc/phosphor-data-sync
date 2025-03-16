// SPDX-License-Identifier: Apache-2.0

#include "external_data_ifaces.hpp"

namespace data_sync::ext_data
{

// NOLINTNEXTLINE
sdbusplus::async::task<> ExternalDataIFaces::startExtDataFetches()
{
    // NOLINTNEXTLINE
    co_return co_await sdbusplus::async::execution::when_all(
        fetchBMCRedundancyMgrProps(), fetchSiblingBmcPort(),
        fetchRbmcCredentials());
}

BMCRole ExternalDataIFaces::bmcRole() const
{
    return _bmcRole;
}

void ExternalDataIFaces::bmcRole(const BMCRole& bmcRole)
{
    _bmcRole = bmcRole;
}

BMCRedundancy ExternalDataIFaces::bmcRedundancy() const
{
    return _bmcRedundancy;
}

void ExternalDataIFaces::bmcRedundancy(const BMCRedundancy& bmcRedundancy)
{
    _bmcRedundancy = bmcRedundancy;
}

const SiblingBmcPort& ExternalDataIFaces::siblingBmcPort() const
{
    return _siblingBmcPort;
}

void ExternalDataIFaces::siblingBmcPort(const SiblingBmcPort& siblingBmcPort)
{
    _siblingBmcPort = siblingBmcPort;
}

void ExternalDataIFaces::rbmcCredentials(const RbmcCredentials& rbmcCredentials)
{
    _rbmcCredentials = rbmcCredentials;
}

const RbmcCredentials& ExternalDataIFaces::rbmcCredentials() const
{
    return _rbmcCredentials;
}
} // namespace data_sync::ext_data
