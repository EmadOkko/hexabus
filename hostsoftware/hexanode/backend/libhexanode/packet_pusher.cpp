#include "packet_pusher.hpp"
#include <libhexabus/error.hpp>
#include <libhexabus/crc.hpp>
#include <boost/lexical_cast.hpp>
#include "../../../shared/endpoints.h"

using namespace hexanode;

void PacketPusher::deviceInfoReceived(const boost::asio::ip::address_v6& device, const hexabus::Packet& info)
{
	if (_unidentified_devices.count(device) == 0) {
		return;
	}

	typedef std::map<uint32_t, std::string>::const_iterator iter;
	std::map<uint32_t, std::string> eids = _unidentified_devices[device];

	for (iter it = eids.begin(), end = eids.end(); it != end; ++it) {
		std::string sensor_id = sensorID(device, it->first);

		target << "Creating sensor " << sensor_id << std::endl;
		int min_value = 0;
		int max_value = 100;
		/*
		 * TODO: Hack for intersolar, clean things up. This should reside in 
		 * a separate configuration file (propertytree parser)
		 */
		switch(it->first) {
			case EP_POWER_METER: min_value = 0; max_value = 3200; break;
			case EP_TEMPERATURE: min_value = 15; max_value = 30; break;
			case EP_HUMIDITY: min_value = 0; max_value = 100; break;
			case EP_PRESSURE: min_value = 900; max_value = 1050; break;
		}
		hexabus::EndpointRegistry::const_iterator ep_it = _ep_registry.find(it->first);
		const hexabus::EndpointDescriptor& desc = ep_it != _ep_registry.end()
			? ep_it->second
			: hexabus::EndpointDescriptor(it->first, "", boost::none, hexabus::HXB_DTYPE_FLOAT, hexabus::EndpointDescriptor::read, hexabus::EndpointDescriptor::sensor);
		hexanode::Sensor new_sensor(device,
				desc,
				static_cast<const hexabus::EndpointInfoPacket&>(info).value(),
				min_value, max_value,
				desc.type());
		_sensors.insert(std::make_pair(sensor_id, new_sensor));
		try {
			new_sensor.put(_client, _api_uri, it->second); 
		} catch (const std::exception& e) {
			target << "Error defining sensor " << device << "(" << it->first << "): " << e.what() << std::endl;
		}
	}

	_unidentified_devices.erase(device);
}

void PacketPusher::deviceInfoError(const boost::asio::ip::address_v6& device, const hexabus::GenericException& error)
{
	_unidentified_devices.erase(device);
	target << "No reply to device descriptor EPQuery from " << device << ": " << error.what() << std::endl;
}

void PacketPusher::defineSensor(const std::string& sensor_id, uint32_t eid, const std::string& value)
{
	switch (eid) {
		case EP_PV_PRODUCTION:
		case EP_POWER_BALANCE:
		case EP_BATTERY_BALANCE:
			{
				target << "No information regarding sensor " << sensor_id 
					<< " found - creating sensor with boilerplate data." << std::endl;
				int min_value = 0;
				int max_value = 0;
				std::string unit("W");
				std::string name;
				switch(eid) {
					case EP_PV_PRODUCTION:
						min_value = 0;
						max_value = 5000;
						name = "PV Production";
						break;

					case EP_POWER_BALANCE:
						min_value = -10000;
						max_value = 10000;
						name = "Power Balance";
						break;

					case EP_BATTERY_BALANCE:
						min_value = -5000;
						max_value = 5000;
						name = "Battery";
						break;
				}
				hexanode::Sensor new_sensor(_endpoint.address().to_v6(),
						hexabus::EndpointDescriptor(eid, name, unit, hexabus::HXB_DTYPE_FLOAT, hexabus::EndpointDescriptor::read, hexabus::EndpointDescriptor::sensor),
						name,
						min_value, max_value,
						hexabus::HXB_DTYPE_FLOAT
						);
				_sensors.insert(std::make_pair(sensor_id, new_sensor));
				new_sensor.put(_client, _api_uri, value);
			}
			break;

		default:
			boost::asio::ip::address_v6 addr = _endpoint.address().to_v6();
			if (_unidentified_devices.count(addr) == 0) {
				_info.send_request(
						addr,
						hexabus::EndpointQueryPacket(EP_DEVICE_DESCRIPTOR),
						hexabus::filtering::isEndpointInfo() && hexabus::filtering::eid() == EP_DEVICE_DESCRIPTOR,
						boost::bind(&PacketPusher::deviceInfoReceived, this, addr, _1),
						boost::bind(&PacketPusher::deviceInfoError, this, addr, _1));
				_unidentified_devices.insert(std::make_pair(_endpoint.address().to_v6(), std::map<uint32_t, std::string>()));
			}
			_unidentified_devices[addr][eid] = value;
			break;
	}
}

void PacketPusher::push_value(uint32_t eid, const std::string& value)
{
  std::string sensor_id = sensorID(_endpoint.address().to_v6(), eid);
	try {
		std::map<std::string, hexanode::Sensor>::iterator sensor = _sensors.find(sensor_id);
		if (sensor != _sensors.end()) {
			sensor->second.post_value(_client, _api_uri, value);
		} else {
			target << "Sensor " << sensor_id << " not found, defining" << std::endl;
			defineSensor(sensor_id, eid, value);
		}
	} catch (const hexanode::CommunicationException& e) {
		// An error occured during network communication.
		// remove local copy of the sensor and redefine
		_sensors.erase(sensor_id);
		defineSensor(sensor_id, eid, value);
		target << "Removed sensor " << sensor_id << " from cache" << std::endl;
	} catch (const std::exception& e) {
		target << "Attempting to recover from error: " << e.what() << std::endl;
	}
}
