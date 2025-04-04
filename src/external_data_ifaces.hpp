// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/State/BMC/Redundancy/common.hpp>

namespace data_sync::ext_data
{

using RBMC = sdbusplus::common::xyz::openbmc_project::state::bmc::Redundancy;
using BMCRole = RBMC::Role;
using BMCRedundancy = bool;
using SiblingBmcIP = std::string;
using RbmcUserName = std::string;
using RbmcPassword = std::string;
using RbmcCredentials = std::pair<RbmcUserName, RbmcPassword>;

/**
 * @class ExternalDataIFaces
 *
 * @brief This abstract class defines a set of interfaces to retrieve necessary
 *        information from external sources, such as DBus or FileSystems or
 *        other places which may be hosted or modified by various applications.
 *        It enables seamless use of the required information in the logic
 *        without concern for how or when it is retrieved.
 *
 *        The defined interface can be implemented by a derived class for use
 *        in the product, while a mocked class can be utilized for unit testing.
 */
class ExternalDataIFaces
{
  public:
    ExternalDataIFaces() = default;
    ExternalDataIFaces(const ExternalDataIFaces&) = delete;
    ExternalDataIFaces& operator=(const ExternalDataIFaces&) = delete;
    ExternalDataIFaces(ExternalDataIFaces&&) = delete;
    ExternalDataIFaces& operator=(ExternalDataIFaces&&) = delete;
    virtual ~ExternalDataIFaces() = default;

    /**
     * @brief Start fetching external dependent data's
     */
    sdbusplus::async::task<> startExtDataFetches();

    /**
     * @brief Used to obtain the BMC role.
     *
     * @return The BMC role
     */
    BMCRole bmcRole() const;

    /**
     * @brief Used to obtain the BMC redundancy flag.
     *
     * @return The BMC redundancy flag
     */
    BMCRedundancy bmcRedundancy() const;

    /**
     * @brief Used to obtain the Sibling BMC IP.
     *
     * @return The Sibling BMC IP
     */
    const SiblingBmcIP& siblingBmcIP() const;

    /**
     * @brief Used to obtain the BMC username and password
     *
     * @return BMC Username and Password
     */
    const RbmcCredentials& rbmcCredentials() const;

  protected:
    /**
     * @brief Used to retrieve the BMC role.
     */
    virtual sdbusplus::async::task<> fetchBMCRedundancyMgrProps() = 0;

    /**
     * @brief Used to retrieve the Sibling BMC IP.
     */
    virtual sdbusplus::async::task<> fetchSiblingBmcIP() = 0;

    /**
     * @brief Used to retrieve the BMC Username and Password.
     */
    virtual sdbusplus::async::task<> fetchRbmcCredentials() = 0;

    /**
     * @brief A utility API to assign the retrieved BMC role.
     *
     * @param[in] bmcRole - The retrieved BMC role.
     *
     * @return None.
     */
    void bmcRole(const BMCRole& bmcRole);

    /**
     * @brief A utility API to assign the retrieved BMC redundancy flag.
     *
     * @param[in] bmcRole - The retrieved BMC redundancy flag.
     *
     * @return None.
     */
    void bmcRedundancy(const BMCRedundancy& bmcRedundancy);

    /**
     * @brief A utility API to assign the retrieved Sibling BMC IP.
     *
     * @param[in] siblingBmcIP - The retrieved Sibling BMC IP.
     *
     * @return None.
     */
    void siblingBmcIP(const SiblingBmcIP& siblingBmcIP);

    /**
     * @brief A utility API to assign the retrieved BMC Username and Password.
     *
     * @param[in] rbmcCredentials - The retrieved Sibling BMC Username and
     *                              Password.
     *
     * @return None.
     */
    void rbmcCredentials(const RbmcCredentials& rbmcCredentials);

  private:
    /**
     * @brief Holds the BMC role.
     */
    BMCRole _bmcRole{BMCRole::Unknown};

    /**
     * @brief Indicates whether BMC redundancy is enabled in the system.
     */
    BMCRedundancy _bmcRedundancy{false};

    /**
     * @brief hold the Sibling BMC IP
     */
    SiblingBmcIP _siblingBmcIP;

    /**
     * @brief This is Pair, hold the BMCs Username and Password
     */
    RbmcCredentials _rbmcCredentials;
};

} // namespace data_sync::ext_data
