#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/foreach.hpp>
#include <libhexabus/common.hpp>
#include <libhexabus/packet.hpp>
#include <libhexabus/error.hpp>
#include <libhexabus/socket.hpp>
#include <libhexabus/endpoint_registry.hpp>

#include <libhbc/ast_datatypes.hpp>
#include <libhbc/skipper.hpp>
#include <libhbc/hbcomp_grammar.hpp>
#include "../../../shared/endpoints.h"
#include "../../../shared/hexabus_definitions.h"
#include "../../../shared/hexabus_types.h"

namespace po = boost::program_options;

struct endpoint_descriptor 
{
	uint32_t eid;
	uint8_t datatype;
	std::string name;

	bool operator<(const endpoint_descriptor &b) const
	{
		return (eid < b.eid);
	}
};

struct device_descriptor
{
	boost::asio::ip::address_v6 ipv6_address;
	std::string name;
	std::set<uint32_t> endpoint_ids;

	bool operator<(const device_descriptor &b) const
	{
		return (ipv6_address < b.ipv6_address);
	}
};

struct ReceiveCallback { // callback for device discovery
	std::set<boost::asio::ip::address_v6>* addresses;
	void operator()(const hexabus::Packet& packet, const boost::asio::ip::udp::endpoint asio_ep)
	{
		addresses->insert(asio_ep.address().to_v6());
	}
};

struct InfoCallback { // callback for populating data structures
	device_descriptor* device;
	std::set<endpoint_descriptor>* endpoints;
	bool* received;
	void operator()(const hexabus::Packet& packet, const boost::asio::ip::udp::endpoint asio_ep)
	{
		const hexabus::EndpointInfoPacket* endpointInfo = dynamic_cast<const hexabus::EndpointInfoPacket*>(&packet);
		const hexabus::InfoPacket<uint32_t>* info = dynamic_cast<const hexabus::InfoPacket<uint32_t>*>(&packet);
		if(endpointInfo != NULL)
		{
			if(endpointInfo->eid() == EP_DEVICE_DESCRIPTOR) // If it's endpoint 0 (device descriptor), it contains the device name.
			{
				device->name = endpointInfo->value();
			} else { // otherwise it contains information about an endpoint, so build an endpoint_descriptor and add it to the list
				endpoint_descriptor ep;
				ep.eid = endpointInfo->eid();
				ep.datatype = endpointInfo->datatype();
				ep.name = endpointInfo->value();

				endpoints->insert(ep);
			}
			*received = true;
		}

		if(info != NULL)
		{
			if ((info->eid() % 32) == 0) // we only care for the device descriptors
			{
				uint32_t val = info->value() >> 1; // skip device descriptor
				for(int i = 1; i < 32; ++i)
				{
					if(val & 1) // find out whether LSB is set
						device->endpoint_ids.insert(info->eid() + i); // if it's set, store the EID (the bit's position in the device descriptor).

					val >>= 1; // right-shift in order to have the next EID in the LSB
				}
			}
			*received = true;
		}

	}
};

struct ErrorCallback {
	void operator()(const hexabus::GenericException& error)
	{
		std::cerr << "Error receiving packet: " << error.what() << std::endl;
		exit(1);
	}
};

void print_dev_info(const device_descriptor& dev)
{
	std::cout << "Device information:" << std::endl;
	std::cout << "\tIP address: \t" << dev.ipv6_address.to_string() << std::endl;
	std::cout << "\tDevice name: \t" << dev.name << std::endl;
	std::cout << "\tEndpoints: \t";
	for(std::set<uint32_t>::const_iterator it = dev.endpoint_ids.begin(); it != dev.endpoint_ids.end(); ++it)
		std::cout << *it << " ";
	std::cout << std::endl;
}

void print_ep_info(const endpoint_descriptor& ep)
{
	std::cout << "Endpoint Information:" << std::endl;
	std::cout << "\tEndpoint ID:\t" << ep.eid << std::endl;
	std::cout << "\tData type:\t";
	switch(ep.datatype)
	{
		case hexabus::HXB_DTYPE_BOOL:
			std::cout << "Bool"; break;
		case hexabus::HXB_DTYPE_UINT8:
			std::cout << "UInt8"; break;
		case hexabus::HXB_DTYPE_UINT32:
			std::cout << "UInt32"; break;
		case hexabus::HXB_DTYPE_DATETIME:
			std::cout << "Datetime"; break;
		case hexabus::HXB_DTYPE_FLOAT:
			std::cout << "Float"; break;
		case hexabus::HXB_DTYPE_TIMESTAMP:
			std::cout << "Timestamp"; break;
		case hexabus::HXB_DTYPE_128STRING:
			std::cout << "String"; break;
		case hexabus::HXB_DTYPE_16BYTES:
			std::cout << "Binary (16bytes)"; break;
		case hexabus::HXB_DTYPE_65BYTES:
			std::cout << "Binary (65bytes)"; break;
		default:
			std::cout << "(unknown)"; break;
	}
	std::cout << std::endl;
	std::cout << "\tName:\t\t" << ep.name <<std::endl;
	std::cout << std::endl;
}

std::string remove_specialchars(std::string s)
{
	size_t space;
	while( // only leave in the characters that Hexabus Compiler can handle.
		(space = s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"))
		!= std::string::npos
		)
		s = s.erase(space, 1);

	// the first character can't be a number. If it is, delete it.
	while(std::string("0123456789").find(s[0]) != std::string::npos)
		s = s.substr(1);

	// device/endpoint name can't be empty. Let's make it "_empty_" instead.
	if(s.length() == 0)
		s = "_empty_";

	return s;
}

void write_dev_desc(const device_descriptor& dev, std::ostream& target)
{
	target << "# Auto-generated by hexinfo." << std::endl;
	target << "device " << remove_specialchars(dev.name) << " {" << std::endl;
	target << "\tip " << dev.ipv6_address.to_string() << ";" << std::endl;
	target << "\teids { ";
	for(std::set<uint32_t>::const_iterator it = dev.endpoint_ids.begin(); it != dev.endpoint_ids.end(); ) // no increment here!
	{
		target << *it;
		if(++it != dev.endpoint_ids.end()) // increment here to see if we have to put a comma
			target << ", ";
	}
	target << " }" << std::endl;
	target << "}" << std::endl << std::endl;
}

struct json_string_writer {
	std::string str;
	json_string_writer(const std::string& str) : str(str) {}

	friend std::ostream& operator<<(std::ostream& target, const json_string_writer& jsw)
	{
		for (std::string::const_iterator it = jsw.str.begin(), end = jsw.str.end(); it != end; it++) {
			switch (*it) {
				case '"': target << "\\\""; break;
				case '\n': target << "\\n"; break;
				case '\r': target << "\\r"; break;
				case '\t': target << "\\t"; break;
				case '\v': target << "\\v"; break;
				default: target << *it;
			}
		}
		return target;
	}
};

json_string_writer json(const std::string& str)
{
	return json_string_writer(str);
}

bool write_dev_desc_json(const device_descriptor& dev, std::ostream& target, bool emit_comma)
{
	static hexabus::EndpointRegistry ep_registry;

	// if name == "", we might as well assume temporary communication problems
	if (dev.name.length() != 0) {
		if (emit_comma) {
			target << "," << std::endl;
		}
		target
			<< "{" << std::endl
			<< "\t\"name\": \"" << json(dev.name) << "\"," << std::endl
			<< "\t\"ip\": \"" << dev.ipv6_address << "\"," << std::endl
			<< "\t\"endpoints\": [" << std::endl;
		for (std::set<uint32_t>::const_iterator it = dev.endpoint_ids.begin(), end = dev.endpoint_ids.end(); it != end; ) {
			// device descriptors are not interesting
			if (*it % 32 == 0) {
				it++;
				continue;
			}

			target
				<< "\t\t{" << std::endl
				<< "\t\t\t\"eid\": " << *it << "," << std::endl;

			hexabus::EndpointRegistry::const_iterator ep_it = ep_registry.find(*it);
			if (ep_it != ep_registry.end()) {
				target
					<< "\t\t\t\"unit\": \"" << json(ep_it->second.unit().get_value_or("")) << "\"," << std::endl
					<< "\t\t\t\"description\": \"" << json(ep_it->second.description()) << "\"," << std::endl
					<< "\t\t\t\"function\": \"";
				switch (ep_it->second.function()) {
					case hexabus::EndpointDescriptor::sensor:         target << "sensor"; break;
					case hexabus::EndpointDescriptor::actor:          target << "actor"; break;
					case hexabus::EndpointDescriptor::infrastructure: target << "infrastructure"; break;
				}
				target << "\"," << std::endl
					<< "\t\t\t\"type\": " << ep_it->second.type() << std::endl;
			} else {
			}

			target << "\t\t}";

			it++;
			if (it != end) {
				target << ",";
			}
			target << std::endl;
		}
		target
			<< "\t]" << std::endl
			<< "}" << std::endl;

		return true;
	} else {
		return false;
	}
}

void write_ep_desc(const endpoint_descriptor& ep, std::ostream& target)
{
	if(ep.datatype == hexabus::HXB_DTYPE_BOOL     // only write output for datatypes the Hexabs Compiler can handle
		|| ep.datatype == hexabus::HXB_DTYPE_UINT8
		|| ep.datatype == hexabus::HXB_DTYPE_UINT32
		|| ep.datatype == hexabus::HXB_DTYPE_FLOAT)
	{
		target << "# Auto-generated by hexinfo." << std::endl;
		target << "endpoint " << remove_specialchars(ep.name) << " {" << std::endl;
		target << "\teid " << ep.eid << ";" << std::endl;
		target << "\tdatatype ";
		switch(ep.datatype)
		{
			case hexabus::HXB_DTYPE_BOOL:
				target << "BOOL"; break;
			case hexabus::HXB_DTYPE_UINT8:
				target << "UINT8"; break;
			case hexabus::HXB_DTYPE_UINT32:
				target << "UINT32"; break;
			case hexabus::HXB_DTYPE_FLOAT:
				target << "FLOAT"; break;
			default:
				target << "(unknown)"; break;
		}
		target << ";" << std::endl;
		target << "\taccess { broadcast, write } #TODO auto-generated template. Please edit!" << std::endl;
		target << "}" << std::endl << std::endl;
	}
}

void send_packet(hexabus::Socket* net, const boost::asio::ip::address_v6& addr, const hexabus::Packet& packet)
{
	try {
		net->send(packet, addr);
	} catch (const hexabus::NetworkException& e) {
		std::cerr << "Could not send packet to " << addr << ": " << e.code().message() << std::endl;
		exit(1);
	}
}

hexabus::hbc_doc read_file(std::string filename, bool verbose)
{
	hexabus::hbc_doc ast; // The AST

	// read the file, and parse the endpoint definitions
	bool r = false;
	std::ifstream in(filename.c_str(), std::ios_base::in);
	if(verbose)
		std::cout << "Reading input file " << filename << "..." << std::endl;

	if(!in) {
		std::cerr << "Could not open input file: " << filename << " -- does it exist? (I will try to create it for you!)" << std::endl;
		return ast;
	}

	in.unsetf(std::ios::skipws); // no white space skipping, this will be handled by the parser

	typedef std::istreambuf_iterator<char> base_iterator_type;
	typedef boost::spirit::multi_pass<base_iterator_type> forward_iterator_type;
	typedef classic::position_iterator2<forward_iterator_type> pos_iterator_type;

	base_iterator_type in_begin(in);
	forward_iterator_type fwd_begin = boost::spirit::make_default_multi_pass(in_begin);
	forward_iterator_type fwd_end;
	pos_iterator_type position_begin(fwd_begin, fwd_end, filename);
	pos_iterator_type position_end;

	std::vector<std::string> error_hints;
	typedef hexabus::hexabus_comp_grammar<pos_iterator_type> hexabus_comp_grammar;
	typedef hexabus::skipper<pos_iterator_type> Skip;
	hexabus_comp_grammar grammar(error_hints, position_begin);
	Skip skipper;

	using boost::spirit::ascii::space;
	using boost::spirit::ascii::char_;
	try {
		r = phrase_parse(position_begin, position_end, grammar, skipper, ast);
	} catch(const qi::expectation_failure<pos_iterator_type>& e) {
		const classic::file_position_base<std::string>& pos = e.first.get_position();
		std::cerr << "Error in " << pos.file << " line " << pos.line << " column " << pos.column << std::endl
			<< "'" << e.first.get_currentline() << "'" << std::endl
			<< std::setw(pos.column) << " " << "^-- " << *hexabus::error_traceback_t::stack.begin() << std::endl;
	} catch(const std::runtime_error& e) {
		std::cout << "Exception occured: " << e.what() << std::endl;
	}

	if(r && position_begin == position_end) {
		if(verbose)
			std::cout << "Parsing of file " << filename << " succeeded." << std::endl;
	} else {
		std::cerr << "Parsing of file " << filename << " failed." << std::endl;
		exit(1);
	}

	return ast;
}

int main(int argc, char** argv)
{
	// the command line interface
	std::ostringstream oss;
	oss << "Usage: " << argv[0] << " IP [additional options]";
	po::options_description desc(oss.str());
	desc.add_options()
		("help,h", "produce help message")
		("version", "print version and exit")
		("ip,i", po::value<std::string>(), "IP address of device")
		("interface,I", po::value<std::string>(), "Interface to send multicast from")
		("discover,c", "automatically discover hexabus devices")
		("print,p", "print device and endpoint info to the console")
		("epfile,e", po::value<std::string>(), "name of Hexabus Compiler header file to write the endpoint list to")
		("devfile,d", po::value<std::string>(), "name of Hexabus Compiler header file to write the device definition to")
		("json,j", "use JSON as output format")
		("verbose,V", "print more status information")
		;

	po::positional_options_description p;
	p.add("ip", 1);

	po::variables_map vm;

	// begin processing of command line parameters
	try {
		po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
		po::notify(vm);
	} catch (std::exception& e) {
		std::cerr << "Cannot process command line: " << e.what() << std::endl;
		exit(-1);
	}

	if(vm.count("help"))
	{
		std::cout << desc << std::endl;
		return 1;
	}

	if(vm.count("version"))
	{
		std::cout << "hexinfo -- hexabus endpoint enumeration tool" << std::endl;
		std::cout << "libhexabus version " << hexabus::version() << std::endl;
		return 0;
	}

	bool verbose = false;
	if(vm.count("verbose"))
		verbose = true;

	// set up the things we need later =========
	std::set<boost::asio::ip::address_v6> addresses; // Holds the list of addresses to scan -- either filled with everything we "discover" or with just one "ip"
	// init the network interface
	boost::asio::io_service io;
	hexabus::Socket* network = new hexabus::Socket(io);
	try {
		if(vm.count("interface")) {
			if(verbose)
				std::cout << "Using network interface " << vm["interface"].as<std::string>() << "." << std::endl;
			network->mcast_from(vm["interface"].as<std::string>());
		}
	} catch(const hexabus::NetworkException& e) {
		std::cerr << "Could not open socket: " << e.code().message() << std::endl;
		return 1;
	}
	boost::asio::ip::address_v6 bind_addr(boost::asio::ip::address_v6::any());
	network->bind(bind_addr);
	
	const unsigned int NUM_RETRIES = 5;
	// sets for storing the received data
	std::set<device_descriptor> devices;
	std::set<endpoint_descriptor> endpoints;
	// =========================================


	if(vm.count("ip") && vm.count("discover"))
	{
		std::cerr << "Error: Options --ip and --discover are mutually exclusive." << std::endl;
		exit(1);
	}

	if(vm.count("discover"))
	{
		if(verbose)
			std::cout << "Discovering devices..." << std::endl;
		// send the packet
		boost::asio::ip::address_v6 hxb_broadcast_address = boost::asio::ip::address_v6::from_string(HXB_GROUP);
		send_packet(network, hxb_broadcast_address, hexabus::QueryPacket(EP_DEVICE_DESCRIPTOR));

		// call back handlers for receiving packets
		ReceiveCallback receiveCallback = { &addresses };
		ErrorCallback errorCallback;

		// use connections so that callback handlers get deleted (.disconnect())
		boost::signals2::connection c1 = network->onAsyncError(errorCallback);
		boost::signals2::connection c2 = network->onPacketReceived(receiveCallback, hexabus::filtering::isInfo<uint32_t>() && (hexabus::filtering::eid() == EP_DEVICE_DESCRIPTOR));

		// timer that cancels receiving after n seconds
		boost::asio::deadline_timer timer(network->ioService());
		timer.expires_from_now(boost::posix_time::seconds(3)); // TODO do we want this configurable? Or at least as a constant
		timer.async_wait(boost::bind(&boost::asio::io_service::stop, &io));

		boost::asio::deadline_timer resend_timer1(network->ioService()); // resend packet twice, with 1 sec timout in between
		boost::asio::deadline_timer resend_timer2(network->ioService());
		resend_timer1.expires_from_now(boost::posix_time::seconds(1));
		resend_timer2.expires_from_now(boost::posix_time::seconds(2));
		resend_timer1.async_wait(boost::bind(&send_packet, network, hxb_broadcast_address, hexabus::QueryPacket(EP_DEVICE_DESCRIPTOR)));
		resend_timer2.async_wait(boost::bind(&send_packet, network, hxb_broadcast_address, hexabus::QueryPacket(EP_DEVICE_DESCRIPTOR)));

		network->ioService().run();

		c1.disconnect();
		c2.disconnect();

		if(verbose)
		{
			std::cout << "Discovered addresses:" << std::endl;
			for(std::set<boost::asio::ip::address_v6>::iterator it = addresses.begin(); it != addresses.end(); ++it)
			{
				std::cout << "\t" << it->to_string() << std::endl;
			}
			std::cout << std::endl;
		}
	} else if(vm.count("ip")) {
		addresses.insert(boost::asio::ip::address_v6::from_string(vm["ip"].as<std::string>()));
	} else {
		std::cerr << "You must either specify an IP address or use the --discover option." << std::endl;
		exit(1);
	}

	for(std::set<boost::asio::ip::address_v6>::iterator address_it = addresses.begin(); address_it != addresses.end(); ++address_it)
	{
		device_descriptor device;
		boost::asio::ip::address_v6 target_ip = *address_it;
		device.ipv6_address = target_ip;

		if(verbose)
			std::cout << "Querying device " << target_ip.to_string() << "..." << std::endl;

		// callback handler for incoming (ep)info packets
		bool received = false;
		InfoCallback infoCallback = { &device, &endpoints, &received };
		ErrorCallback errorCallback;
		boost::signals2::connection c1 = network->onAsyncError(errorCallback);
		boost::signals2::connection c2 = network->onPacketReceived(infoCallback,
			(hexabus::filtering::isInfo<uint32_t>() && (hexabus::filtering::eid() % 32 == 0))
			|| hexabus::filtering::isEndpointInfo());

		unsigned int retries = 0;

		// epquery the dev.descriptor, to get the name of the device
		while(!received && retries < NUM_RETRIES)
		{
			// send the device name query packet and listen for the reply
			if(verbose)
				std::cout << "Sending EPQuery." << std::endl;
			send_packet(network, target_ip, hexabus::EndpointQueryPacket(EP_DEVICE_DESCRIPTOR));

			boost::asio::deadline_timer timer(network->ioService()); // wait for a second
			timer.expires_from_now(boost::posix_time::milliseconds(350));
			timer.async_wait(boost::bind(&boost::asio::io_service::stop, &io));

			network->ioService().reset();
			network->ioService().run();

			retries++;
		}

		if(!received && verbose)
			std::cout << "No reply on device descriptor EPQuery from " << target_ip.to_string() << std::endl;

		for (uint32_t ep_desc = 0; ep_desc < 256; ep_desc += 32) {
			received = false;
			retries = 0;

			// query the dev.descriptor, to build a list of endpoints
			while (!received && retries < NUM_RETRIES) {
				if (verbose)
					std::cout << "Sending descriptor query for " << ep_desc << "." << std::endl;
				// send the device descriptor (endpoint list) query packet and listen for the reply
				send_packet(network, target_ip, hexabus::QueryPacket(ep_desc));

				boost::asio::deadline_timer timer(network->ioService()); // wait for a second
				timer.expires_from_now(boost::posix_time::milliseconds(350));
				timer.async_wait(boost::bind(&boost::asio::io_service::stop, &io));

				network->ioService().reset();
				network->ioService().run();

				retries++;
			}

			if(!received && verbose)
				std::cout << "No reply on device descriptor Query from " << target_ip.to_string() << std::endl;
		}

		// step 1: Make a set of endpoints to query.
		std::multiset<uint32_t> eps_to_query;
		BOOST_FOREACH(uint32_t endpoint_id, device.endpoint_ids)
		{
			// put each EP into this list three times, so that it is queried at maximum three times
			for(unsigned int i = 0; i < NUM_RETRIES; i++)
				eps_to_query.insert(endpoint_id);
		}

		// step 2: Query each entry of the set, and remove it.
		while(eps_to_query.size())
		{
			received = false;
			uint32_t eid = *eps_to_query.begin();
			eps_to_query.erase(eps_to_query.begin());

			if(verbose)
				std::cout << "Sending EPQuery to EID " << eid << std::endl;

			send_packet(network, target_ip, hexabus::EndpointQueryPacket(eid));

			boost::asio::deadline_timer timer(network->ioService()); // wait for a second
			timer.expires_from_now(boost::posix_time::milliseconds(350));
			timer.async_wait(boost::bind(&boost::asio::io_service::stop, &io));

			network->ioService().reset();
			network->ioService().run();

			// step 2a: If a reply is received for this EP, remove it completely from the set.
			if(received)
				eps_to_query.erase(eid);
		}

		c1.disconnect();
		c2.disconnect();

		// step 3: Now we have a set of endpoint descriptors.

		if(vm.count("print"))
		{
			// print the information onto the command line
			print_dev_info(device);
			std::cout << std::endl;
			for(std::set<endpoint_descriptor>::iterator it = endpoints.begin(); it != endpoints.end(); ++it)
				if (device.endpoint_ids.count(it->eid))
					print_ep_info(*it);
		}

		devices.insert(device);
	}

	if(vm.count("epfile"))
	{
		// read in HBC file
		hexabus::hbc_doc hbc_input = read_file(vm["epfile"].as<std::string>(), verbose);

		std::set<uint32_t> existing_eids;
		// check all endpoint definitions, insert eids into set of existing eids
		BOOST_FOREACH(hexabus::hbc_block block, hbc_input.blocks)
		{
			if(block.which() == 1) // endpoint_doc
			{
				BOOST_FOREACH(hexabus::endpoint_cmd_doc epc, boost::get<hexabus::endpoint_doc>(block).cmds)
				{
					if(epc.which() == 0) // ep_eid_doc
					{
						existing_eids.insert(boost::get<hexabus::ep_eid_doc>(epc).eid);
					}
				}
			}
		}

		if(verbose)
		{
			std::cout << "Endpoint IDs already present in file: ";
			for(std::set<uint32_t>::iterator it = existing_eids.begin(); it != existing_eids.end(); ++it)
			{
				std::cout << *it << " ";
			}
			std::cout << std::endl;
		}

		// Write (newly discovered) eids to the file
		std::ofstream ofs;
		ofs.open(vm["epfile"].as<std::string>().c_str(), std::fstream::app);
		if(!ofs)
		{
			std::cerr << "Error: Could not open output file: " << vm["epfile"].as<std::string>().c_str() << std::endl;
			return 1;
		}

		for(std::set<endpoint_descriptor>::iterator it = endpoints.begin(); it != endpoints.end(); ++it)
		{
			if(!existing_eids.count(it->eid))
				write_ep_desc(*it, ofs);
		}
	}

	if(vm.count("devfile"))
	{
		if(vm.count("json"))
		{
			std::ofstream ofs;

			if(vm["devfile"].as<std::string>()=="-") {
				// write to stdout instead of a file
			} else {
				// open file
				ofs.open(vm["devfile"].as<std::string>().c_str(), std::fstream::out);
			}

			std::ostream& out(vm["devfile"].as<std::string>() == "-" ? std::cout : ofs);

			if(!out)
			{
				std::cerr << "Error: Could not open output file: " << vm["devfile"].as<std::string>().c_str() << std::endl;
				return 1;
			}

			out << "{\"devices\": [" << std::endl;

			bool hasWritten = false;
			for(std::set<device_descriptor>::iterator it = devices.begin(); it != devices.end(); ++it)
			{
				hasWritten |= write_dev_desc_json(*it, out, hasWritten && it != devices.begin());
			}

			out << "]}" << std::endl;

		} else {
			hexabus::hbc_doc hbc_input = read_file(vm["devfile"].as<std::string>(), verbose);

			std::set<boost::asio::ip::address_v6> existing_dev_addresses;
			// check all the device definitions, and store their ip addresses in the set.
			BOOST_FOREACH(hexabus::hbc_block block, hbc_input.blocks)
			{
				if(block.which() == 2) // alias_doc
				{
					BOOST_FOREACH(hexabus::alias_cmd_doc ac, boost::get<hexabus::alias_doc>(block).cmds)
					{
						if(ac.which() == 0) // alias_ip_doc
						{
							existing_dev_addresses.insert(
									boost::asio::ip::address_v6::from_string(
										boost::get<hexabus::alias_ip_doc>(ac).ipv6_address));
						}
					}
				}
			}

			if(verbose)
			{
				std::cout << "Device IPs already present in file: " << std::endl;
				for(std::set<boost::asio::ip::address_v6>::iterator it = existing_dev_addresses.begin();
						it != existing_dev_addresses.end();
						++it)
				{
					std::cout << "\t" << it->to_string() << std::endl;
				}
			}

			// open file
			std::ofstream ofs;
			ofs.open(vm["devfile"].as<std::string>().c_str(), std::fstream::app);
			if(!ofs)
			{
				std::cerr << "Error: Could not open output file: " << vm["devfile"].as<std::string>().c_str() << std::endl;
				return 1;
			}

			for(std::set<device_descriptor>::iterator it = devices.begin(); it != devices.end(); ++it)
			{

				// only insert our device descriptors if the IP is not already found.
				if(!existing_dev_addresses.count(it->ipv6_address))
					write_dev_desc(*it, ofs);
			}
		}
	}
}
