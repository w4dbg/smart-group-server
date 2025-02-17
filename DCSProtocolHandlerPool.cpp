/*
 *   Copyright (C) 2012,2013 by Jonathan Naylor G4KLX
 *   Copyright (c) 2017-2018 by Thomas A. Early N7TAE
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <cassert>

#include "DCSProtocolHandlerPool.h"
#include "Utils.h"

CDCSProtocolHandlerPool::CDCSProtocolHandlerPool()
{
	m_index = m_pool.end();
}

CDCSProtocolHandlerPool::~CDCSProtocolHandlerPool()
{
	while (m_pool.end() != m_pool.begin()) {
		auto it = m_pool.begin();
		delete it->second;
		m_pool.erase(it);
	}
}

CDCSProtocolHandler *CDCSProtocolHandlerPool::getHandler()
{
	CDCSProtocolHandler *proto = new CDCSProtocolHandler(AF_INET);
	if (proto) {
		if (proto->open()) {
			m_pool[proto->getPort()] = proto;
			printf("New CDCSProtocolHandler now on port %u.\n", proto->getPort());
		} else {
			delete proto;
			proto = NULL;
			printf("ERROR: Can't open new DCS port!\n");
		}
	} else
		printf("ERROR: Can't allocate new CDCSProtocolHandler\n");
	return proto;
}

void CDCSProtocolHandlerPool::release(CDCSProtocolHandler *handler)
{
	assert(handler != NULL);
	for (auto it=m_pool.begin(); it!=m_pool.end(); it++) {
		if (it->second == handler) {
			it->second->close();
			delete it->second;
			printf("Releasing CDCSProtocolHandler on port %u.\n", it->first);
			m_pool.erase(it);
			return;
		}
	}
	// we should never get here!
	printf("ERROR: could not find CDCSProtocolHander (port=%u) to release!\n", handler->getPort());
}

DCS_TYPE CDCSProtocolHandlerPool::read()
{
	if (m_index == m_pool.end())
		m_index = m_pool.begin();
	while (m_index != m_pool.end()) {
		DCS_TYPE type = m_index->second->read();
		if (type != DC_NONE)
			return type;
		m_index++;
	}
	return DC_NONE;
}

CAMBEData *CDCSProtocolHandlerPool::readData()
{
	return m_index->second->readData();
}

CPollData *CDCSProtocolHandlerPool::readPoll()
{
	return m_index->second->readPoll();
}

CConnectData *CDCSProtocolHandlerPool::readConnect()
{
	return m_index->second->readConnect();
}

void CDCSProtocolHandlerPool::close()
{
	for (auto it=m_pool.begin(); it!=m_pool.end(); it++)
		it->second->close();
}
