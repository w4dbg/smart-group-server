/*
 *   Copyright (C) 2011-2014 by Jonathan Naylor G4KLX
 *   Copyright (c) 2017,2018 by Thomas A. Early N7TAE
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
#include <cstdio>
#include <cstring>
#include <vector>

#include "SlowDataEncoder.h"
#include "GroupHandler.h"
#include "DExtraHandler.h"		// DEXTRA_LINK
#include "DCSHandler.h"			// DCS_LINK
#include "Utils.h"

const unsigned int MESSAGE_DELAY = 4U;

// define static members
CG2ProtocolHandler *CGroupHandler::m_g2Handler[2] = { NULL, NULL };
CIRCDDB            *CGroupHandler::m_irc[2] = { NULL, NULL };
CCacheManager      *CGroupHandler::m_cache = NULL;
std::string         CGroupHandler::m_gateway;
std::list<CGroupHandler *> CGroupHandler::m_Groups;


CSGSUser::CSGSUser(const std::string &callsign, unsigned int timeout) :
m_callsign(callsign),
m_timer(1000U, timeout)
{
	m_timer.start();
}

CSGSUser::~CSGSUser()
{
}

bool CSGSUser::clock(unsigned int ms)
{
	m_timer.clock(ms);

	return m_timer.isRunning() && m_timer.hasExpired();
}

bool CSGSUser::hasExpired()
{
	return m_timer.isRunning() && m_timer.hasExpired();
}

void CSGSUser::reset()
{
	m_timer.start();
}

std::string CSGSUser::getCallsign() const
{
	return m_callsign;
}

CTimer CSGSUser::getTimer() const
{
	return m_timer;
}

CSGSId::CSGSId(unsigned int id, unsigned int timeout, CSGSUser *user) :
m_id(id),
m_timer(1000U, timeout),
m_login(false),
m_logoff(false),
m_end(false),
m_user(user)
{
	assert(user != NULL);

	m_timer.start();
}

CSGSId::~CSGSId()
{
}

unsigned int CSGSId::getId() const
{
	return m_id;
}

void CSGSId::reset()
{
	m_timer.start();
}

void CSGSId::setLogin()
{
	m_login = true;
}

void CSGSId::setLogoff()
{
	if (!m_login)
		m_logoff = true;
}

void CSGSId::setEnd()
{
	m_end = true;
}

bool CSGSId::clock(unsigned int ms)
{
	m_timer.clock(ms);

	return m_timer.isRunning() && m_timer.hasExpired();
}

bool CSGSId::hasExpired()
{
	return m_timer.isRunning() && m_timer.hasExpired();
}

bool CSGSId::isLogin() const
{
	return m_login;
}

bool CSGSId::isLogoff() const
{
	return m_logoff;
}

bool CSGSId::isEnd() const
{
	return m_end;
}

CSGSUser* CSGSId::getUser() const
{
	return m_user;
}

//CTextCollector& CSGSId::getTextCollector()
//{
	//return m_textCollector;
//}

void CGroupHandler::add(const std::string &callsign, const std::string &logoff, const std::string &repeater, const std::string &infoText, unsigned int userTimeout, CALLSIGN_SWITCH callsignSwitch, bool txMsgSwitch, bool listenOnly, const std::string &reflector)
{
	CGroupHandler *group = new CGroupHandler(callsign, logoff, repeater, infoText, userTimeout, callsignSwitch, txMsgSwitch, listenOnly, reflector);

	if (group)
		m_Groups.push_back(group);
	else
		printf("Cannot allocate Smart Group with callsign %s\n", callsign.c_str());
}

void CGroupHandler::setG2Handler(CG2ProtocolHandler *handler0, CG2ProtocolHandler *handler1)
{
	assert(handler0 != NULL);

	m_g2Handler[0] = handler0;
	m_g2Handler[1] = handler1;
}

void CGroupHandler::setIRC(CIRCDDB *irc0, CIRCDDB *irc1)
{
	assert(irc0 != NULL);

	m_irc[0] = irc0;
	m_irc[1] = irc1;
}

void CGroupHandler::setCache(CCacheManager *cache)
{
	assert(cache != NULL);

	m_cache = cache;
}

void CGroupHandler::setGateway(const std::string &gateway)
{
	m_gateway = gateway;
}

CGroupHandler *CGroupHandler::findGroup(const std::string &callsign)
{
	for (auto it=m_Groups.begin(); it!=m_Groups.end(); it++) {
		if (0 == (*it)->m_groupCallsign.compare(callsign))
			return *it;
	}
	return NULL;
}

CGroupHandler *CGroupHandler::findGroup(const CHeaderData &header)
{
	std::string your = header.getYourCall();

	for (auto it=m_Groups.begin(); it!=m_Groups.end(); it++) {
		if (0 == (*it)->m_groupCallsign.compare(your))
			return *it;
		if (0 == (*it)->m_offCallsign.compare(your))
			return *it;
	}
	return NULL;
}

CGroupHandler *CGroupHandler::findGroup(const CAMBEData &data)
{
	unsigned int id = data.getId();

	for (auto it=m_Groups.begin(); it!=m_Groups.end(); it++) {
		if ((*it)->m_id == id)
			return *it;
	}
	return NULL;
}

std::list<std::string> CGroupHandler::listGroups()
{
	std::list<std::string> groups;

	for (auto it=m_Groups.begin(); it!=m_Groups.end(); it++)
		groups.push_back((*it)->m_groupCallsign);

	return groups;
}

CRemoteGroup *CGroupHandler::getInfo() const
{
	CRemoteGroup *data = new CRemoteGroup(m_groupCallsign, m_offCallsign, m_repeater, m_infoText, m_linkReflector, m_linkStatus, m_userTimeout);

	for (auto it=m_users.begin(); it!=m_users.end(); ++it) {
		CSGSUser* user = it->second;
		data->addUser(user->getCallsign(), user->getTimer().getTimer(), user->getTimer().getTimeout());
	}

	return data;
}

void CGroupHandler::finalise()
{
	while (m_Groups.size()) {
		delete m_Groups.front();
		m_Groups.pop_front();
	}
}

void CGroupHandler::clock(unsigned int ms)
{
	for (auto it=m_Groups.begin(); it!=m_Groups.end(); it++)
		(*it)->clockInt(ms);
}

void CGroupHandler::link()
{
	for (auto it=m_Groups.begin(); it!=m_Groups.end(); it++)
		(*it)->linkInt();
}

CGroupHandler::CGroupHandler(const std::string &callsign, const std::string &logoff, const std::string &repeater, const std::string &infoText, unsigned int userTimeout, CALLSIGN_SWITCH callsignSwitch, bool txMsgSwitch, bool listenOnly, const std::string &reflector) :
m_groupCallsign(callsign),
m_offCallsign(logoff),
m_shortCallsign("SMRT"),
m_repeater(repeater),
m_infoText(infoText),
m_linkReflector(reflector),
m_linkGateway(),
m_linkStatus(LS_NONE),
m_oldlinkStatus(LS_INIT),
m_linkTimer(1000U, NETWORK_TIMEOUT),
m_id(0x00U),
m_announceTimer(1000U, 2U * 60U),		// 2 minutes
m_pingTimer(1000U, 10U),
m_userTimeout(userTimeout),
m_callsignSwitch(callsignSwitch),
m_txMsgSwitch(txMsgSwitch),
m_listenOnly(listenOnly),
m_ids(),
m_users(),
m_repeaters()
{
	m_announceTimer.start();
	m_pingTimer.start();

	// set link type
	if (m_linkReflector.size())
		m_linkType = (0 == m_linkReflector.compare(0, 3, "XRF")) ? LT_DEXTRA : LT_DCS;
	else
		m_linkType = LT_NONE;

	// Create the short version of the Smart Group callsign
	if (0 == m_groupCallsign.compare(0, 3, "SGS")) {
		if (' ' == m_groupCallsign[7])
			m_shortCallsign = std::string("S") + m_groupCallsign.substr(3, 3);
		else
			m_shortCallsign = m_groupCallsign.substr(3, 3) + m_groupCallsign[7];
	}
}

CGroupHandler::~CGroupHandler()
{
	for (auto it = m_ids.begin(); it != m_ids.end(); it++)
		delete it->second;
	m_ids.empty();

	for (auto it = m_users.begin(); it != m_users.end(); ++it)
		delete it->second;
	m_users.empty();

	for (auto it = m_repeaters.begin(); it != m_repeaters.end(); ++it)
		delete it->second;
	m_repeaters.empty();
}

void CGroupHandler::process(CHeaderData &header)
{
	std::string my   = header.getMyCall1();
	std::string your = header.getYourCall();
	unsigned int id  = header.getId();

	CSGSUser *group_user = m_users[my];	// if not found, m_user[my] will be created and its value will be set to NULL
	bool islogin = false;

	// Ensure that this user is in the cache.
	CUserData *userData = m_cache->findUser(my);	// userData is a new record (or NULL), so we have to delete or save it
													// to prevent a memory leak
	if (NULL == userData) {
		if (m_irc[1])
			m_irc[1]->findUser(my);
		m_irc[0]->findUser(my);
	}

	if (0 == your.compare(m_groupCallsign)) {
		// This is a normal message for logging in/relaying
		if (group_user == NULL) {
			printf("Adding %s to Smart Group %s\n", my.c_str(), your.c_str());
			// This is a new user, add him to the list
			group_user = new CSGSUser(my, m_userTimeout * 60U);
			m_users[my] = group_user;

			logUser(LU_ON, your, my);	// inform Quadnet

			// add a new Id for this message
			CSGSId* tx = new CSGSId(id, MESSAGE_DELAY, group_user);
			tx->setLogin();
			m_ids[id] = tx;
			islogin = true;
		} else {
			group_user->reset();

			// Check that it isn't a duplicate header
			CSGSId* tx = m_ids[id];
			if (tx) {
				//printf("Duplicate header from %s, deleting userData...\n", my.c_str());
				delete userData;
				return;
			}
			//printf("Updating %s on Smart Group %s\n", my.c_str(), your.c_str());
			logUser(LU_ON, your, my);	// this will be an update
			m_ids[id] = new CSGSId(id, MESSAGE_DELAY, group_user);
		}
	} else {
		// unsubscribe was sent by someone
		if (userData) {
			delete userData;
			userData = NULL;
		}

		// This is a logoff message
		if (NULL == group_user) {	// Not a known user, ignore
			m_users.erase(my);	// we created it, now we don't need it
			return;
		}

		printf("Removing %s from Smart Group %s\n", group_user->getCallsign().c_str(), m_groupCallsign.c_str());
		logUser(LU_OFF, m_groupCallsign, my);	// inform Quadnet
		// Remove the user from the user list
		m_users.erase(my);

		CSGSId* tx = new CSGSId(id, MESSAGE_DELAY, group_user);
		tx->setLogoff();
		m_ids[id] = tx;

		return;
	}

	if (m_id != 0x00U) {
		delete userData;
		return;
	}

	m_id = id;

	// Change the Your callsign to CQCQCQ
	header.setCQCQCQ();

	header.setFlag1(0x00);
	header.setFlag2(0x00);
	header.setFlag3(0x00);

	if (LT_DEXTRA == m_linkType) {
		if (!islogin && !m_listenOnly) {
			header.setRepeaters(m_linkGateway, m_linkReflector);
			CDExtraHandler::writeHeader(this, header, DIR_OUTGOING);
		}
	} else if (LT_DCS == m_linkType) {
		if (!islogin && !m_listenOnly) {
			header.setRepeaters(m_linkGateway, m_linkReflector);
			CDCSHandler::writeHeader(this, header, DIR_OUTGOING);
		}
	}

	// Get the home repeater of the user, because we don't want to route this incoming back to him
	std::string exclude;
	if (userData) {
		exclude = userData->getRepeater();
		delete userData;	// it's gone now
	}

	// Build new repeater list, based on users that are currently logged in
	for (auto it = m_users.begin(); it != m_users.end(); ++it) {
		CSGSUser *user = it->second;
		if (user != NULL) {
			// Find the user in the cache
			userData = m_cache->findUser(user->getCallsign());

			if (userData) {
				// Check for the excluded repeater
				if (userData->getRepeater().compare(exclude)) {
					// Find the users repeater in the repeater list, add it otherwise
					CSGSRepeater *repeater = m_repeaters[userData->getRepeater()];
					if (repeater == NULL) {
						// Add a new repeater entry
						repeater = new CSGSRepeater;
						// we zone route to all the repeaters, except for the sender who transmitted it
						repeater->m_destination = std::string("/") + userData->getRepeater().substr(0, 6) + userData->getRepeater().back();
						repeater->m_repeater    = userData->getRepeater();
						repeater->m_gateway     = userData->getGateway();
						repeater->m_address     = userData->getAddress();
						m_repeaters[userData->getRepeater()] = repeater;
					}
				}

				delete userData;
				userData = NULL;
			}
		}
	}

	switch (m_callsignSwitch) {
		case SCS_GROUP_CALLSIGN:
			header.setMyCall1(m_groupCallsign);
			header.setMyCall2("SMRT");
			break;
		case SCS_USER_CALLSIGN:
			header.setMyCall1(my);
			header.setMyCall2(m_shortCallsign);
			break;
		default:
			break;
	}
	if (!islogin && !m_listenOnly)
		sendToRepeaters(header);

	if (m_txMsgSwitch && !islogin && !m_listenOnly)
		sendFromText(my);
}

void CGroupHandler::process(CAMBEData &data)
{
	unsigned int id = data.getId();

	CSGSId* tx = m_ids[id];
	if (tx == NULL)
		return;

	tx->reset();

	CSGSUser* user = tx->getUser();
	user->reset();

	if (id == m_id && !tx->isLogin() && !m_listenOnly) {
		if (LT_DEXTRA == m_linkType)
			CDExtraHandler::writeAMBE(this, data, DIR_OUTGOING);
		else if (LT_DCS == m_linkType)
			CDCSHandler::writeAMBE(this, data, DIR_OUTGOING);
		sendToRepeaters(data);
	}

	if (data.isEnd()) {
		if (id == m_id) {
			// Clear the repeater list if we're the relayed id
			for (auto it = m_repeaters.begin(); it != m_repeaters.end(); ++it)
				delete it->second;
			m_repeaters.clear();
			m_id = 0x00U;
		}

		if (tx->isLogin()) {
			tx->reset();
			tx->setEnd();
		} else if (tx->isLogoff()) {
			m_users.erase(user->getCallsign());
			tx->reset();
			tx->setEnd();
		} else {
			m_ids.erase(tx->getId());
			delete tx;
		}
	}
}

bool CGroupHandler::logoff(const std::string &callsign)
{
	if (0 == callsign.compare("ALL     ")) {
		for (auto it = m_users.begin(); it != m_users.end(); ++it) {
			CSGSUser* user = it->second;
			if (user) {
				printf("Removing %s from Smart Group %s, logged off by remote control\n", user->getCallsign().c_str(), m_groupCallsign.c_str());
				logUser(LU_OFF, m_groupCallsign, user->getCallsign());	// inform Quadnet
				delete user;
			}
		}

		for (auto it = m_ids.begin(); it != m_ids.end(); ++it)
			delete it->second;

		for (auto it = m_repeaters.begin(); it != m_repeaters.end(); ++it)
			delete it->second;

		m_users.clear();
		m_ids.clear();
		m_repeaters.clear();

		m_id = 0x00U;

		return true;
	} else {
		CSGSUser* user = m_users[callsign];
		if (user == NULL) {
			printf("Invalid callsign asked to logoff");
			return false;
		}
		printf("Removing %s from Smart Group %s, logged off by remote control\n", user->getCallsign().c_str(), m_groupCallsign.c_str());
		logUser(LU_OFF, m_groupCallsign, user->getCallsign());	// inform Quadnet

		// Find any associated id structure associated with this use, and the logged off user is the
		// currently relayed one, remove his id.
		for (auto it = m_ids.begin(); it != m_ids.end(); ++it) {
			CSGSId* id = it->second;
			if (id != NULL && id->getUser() == user) {
				if (id->getId() == m_id)
					m_id = 0x00U;

				m_ids.erase(it);
				delete id;
				break;
			}
		}

		m_users.erase(callsign);
		delete user;

		// Check to see if we have any users left
		unsigned int count = m_users.size();

		// If none then clear all the data structures
		if (count == 0U) {
			for (auto it = m_ids.begin(); it != m_ids.end(); ++it)
				delete it->second;
			for (auto it = m_repeaters.begin(); it != m_repeaters.end(); ++it)
				delete it->second;

			m_ids.clear();
			m_repeaters.clear();

			m_id = 0x00U;
		}

		return true;
	}
}

bool CGroupHandler::process(CHeaderData &header, DIRECTION, AUDIO_SOURCE)
{
	if (m_id != 0x00U)
		return false;

	std::string my = header.getMyCall1();
	m_id = header.getId();

	m_linkTimer.start();

	// Change the Your callsign to CQCQCQ
	header.setCQCQCQ();

	header.setFlag1(0x00);
	header.setFlag2(0x00);
	header.setFlag3(0x00);

	// Build new repeater list
	for (auto it = m_users.begin(); it != m_users.end(); ++it) {
		CSGSUser* user = it->second;
		if (user) {
			// Find the user in the cache
			CUserData* userData = m_cache->findUser(user->getCallsign());

			if (userData) {
				// Find the users repeater in the repeater list, add it otherwise
				CSGSRepeater* repeater = m_repeaters[userData->getRepeater()];
				if (repeater == NULL) {
					// Add a new repeater entry
					repeater = new CSGSRepeater;
					repeater->m_destination = std::string("/") + userData->getRepeater().substr(0, 6) + userData->getRepeater().back();
					repeater->m_repeater    = userData->getRepeater();
					repeater->m_gateway     = userData->getGateway();
					repeater->m_address     = userData->getAddress();
					m_repeaters[userData->getRepeater()] = repeater;
				}

				delete userData;
				userData = NULL;
			}
		}
	}

	switch (m_callsignSwitch) {
		case SCS_GROUP_CALLSIGN:
			header.setMyCall1(m_groupCallsign);
			header.setMyCall2("SMRT");
			break;
		case SCS_USER_CALLSIGN:
			header.setMyCall1(my);
			header.setMyCall2(m_shortCallsign);
			break;
		default:
			break;
	}

	CSGSId *tx = m_ids[m_id];
	if (tx) {
		if (!tx->isLogin())
			sendToRepeaters(header);
	} else
		sendToRepeaters(header);

	if (m_txMsgSwitch)
		sendFromText(my);

	return true;
}

bool CGroupHandler::process(CAMBEData &data, DIRECTION, AUDIO_SOURCE)
{
	unsigned int id = data.getId();
	if (id != m_id)
		return false;

	m_linkTimer.start();

	CSGSId *tx = m_ids[id];
	if (tx) {
		if (!tx->isLogin())
			sendToRepeaters(data);
	} else
		sendToRepeaters(data);

	if (data.isEnd()) {
		m_linkTimer.stop();
		m_id = 0x00U;

		// Clear the repeater list
		for (auto it = m_repeaters.begin(); it != m_repeaters.end(); ++it)
			delete it->second;
		m_repeaters.clear();
	}

	return true;
}

bool CGroupHandler::remoteLink(const std::string &reflector)
{
	if (LT_NONE != m_linkType)
		return false;

	if (LONG_CALLSIGN_LENGTH != reflector.size())
		return false;
	if (0 == reflector.compare(0, 3, "XRF"))
		m_linkType = LT_DEXTRA;
	else if (0 == reflector.compare(0, 3, "DCS"))
		m_linkType = LT_DCS;
	else
		return false;

	m_linkReflector.assign(reflector);
	return linkInt();
}

bool CGroupHandler::linkInt()
{
	if (LT_NONE == m_linkType)
		return false;

	printf("Linking %s to %s reflector %s\n", m_repeater.c_str(), (LT_DEXTRA==m_linkType)?"DExtra":"DCS", m_linkReflector.c_str());

	// Find the repeater to link to
	CRepeaterData* data = m_cache->findRepeater(m_linkReflector);
	if (data == NULL) {
		printf("Cannot find the reflector in the cache, not linking\n");
		return false;
	}

	m_linkGateway = data->getGateway();
	bool rtv = true;
	switch (m_linkType) {
		case LT_DEXTRA:
			m_linkStatus  = LS_LINKING_DEXTRA;
			CDExtraHandler::link(this, m_repeater, m_linkReflector, data->getAddress());
			break;
		case LT_DCS:
			m_linkStatus  = LS_LINKING_DCS;
			CDCSHandler::link(this, m_repeater, m_linkReflector, data->getAddress());
			break;
		default:
			rtv = false;
			break;
	}
	delete data;
	return rtv;
}

void CGroupHandler::clockInt(unsigned int ms)
{
	m_pingTimer.clock(ms);
	if (m_pingTimer.isRunning() && m_pingTimer.hasExpired()) {
		for (auto it = m_users.begin(); it != m_users.end(); it++) {
			CSGSUser *user = it->second;
			if (user != NULL) {
				std::string addr;
				if (m_cache->findUserAddress(user->getCallsign(), addr)) {
                    int i = 0;
                    if (std::string::npos==addr.find(':') && m_irc[1])
                        i = 1;
					m_g2Handler[i]->writePing(addr);
                }
			}
		}
		m_pingTimer.start();
	}

	m_linkTimer.clock(ms);
	if (m_linkTimer.isRunning() && m_linkTimer.hasExpired()) {
		m_linkTimer.stop();
		m_id = 0x00U;

		// Clear the repeater list
		for (auto it=m_repeaters.begin(); it!=m_repeaters.end(); it++)
			delete it->second;
		m_repeaters.clear();
	}

	m_announceTimer.clock(ms);
	if (m_announceTimer.hasExpired()) {
		for (int i=0; i<2; i++) {
			if (m_irc[i]) {
				m_irc[i]->sendHeardWithTXMsg(m_groupCallsign, "    ", "CQCQCQ  ", m_repeater, m_gateway, 0x00U, 0x00U, 0x00U, std::string(""), m_infoText);
				if (m_offCallsign.size() && m_offCallsign.compare("        "))
					m_irc[i]->sendHeardWithTXMsg(m_offCallsign, "    ", "CQCQCQ  ", m_repeater, m_gateway, 0x00U, 0x00U, 0x00U, std::string(""), m_infoText);
			}
		}
		m_announceTimer.start(60U * 60U);		// 1 hour
	}

	if (m_oldlinkStatus!=m_linkStatus && 7==m_irc[0]->getConnectionState()) {
		updateReflectorInfo();
		m_oldlinkStatus = m_linkStatus;
	}

	// For each incoming id
	for (auto it = m_ids.begin(); it != m_ids.end(); ++it) {
		CSGSId* tx = it->second;

		if (tx != NULL && tx->clock(ms)) {
			std::string callsign = tx->getUser()->getCallsign();

			if (tx->isEnd()) {
				CUserData* user = m_cache->findUser(callsign);
				if (user) {
					if (tx->isLogin()) {
						sendAck(*user, "Logged in");
					} else if (tx->isLogoff()) {
						sendAck(*user, "Logged off");
					}

					delete user;
					user = NULL;
				} else {
					printf("Cannot find %s in the cache", callsign.c_str());
				}

				delete tx;
				m_ids.erase(it);

				// The iterator is now invalid, so we'll find the next expiry on the next clock tick with a
				// new iterator
				break;
			} else {
				if (tx->getId() == m_id) {
					// Clear the repeater list if we're the relayed id
					for (auto it = m_repeaters.begin(); it != m_repeaters.end(); ++it)
						delete it->second;
					m_repeaters.clear();
					m_id = 0x00U;
				}

				if (tx->isLogin()) {
					tx->reset();
					tx->setEnd();
				} else if (tx->isLogoff()) {
					m_users.erase(callsign);
					tx->reset();
					tx->setEnd();
				} else {
					delete tx;
					m_ids.erase(it);
					// The iterator is now invalid, so we'll find the next expiry on the next clock tick with a
					// new iterator
					break;
				}
			}
		}
	}

	// Individual user expiry
	for (auto it = m_users.begin(); it != m_users.end(); ++it) {
		CSGSUser* user = it->second;
		user->clock(ms);
	}

	// Don't do timeouts when relaying audio
	if (m_id != 0x00U)
		return;

	// Individual user expiry
	for (auto it = m_users.begin(); it != m_users.end(); ++it) {
		CSGSUser* user = it->second;
		if (user && user->hasExpired()) {
			printf("Removing %s from Smart Group %s, user timeout\n", user->getCallsign().c_str(), m_groupCallsign.c_str());

			logUser(LU_OFF, m_groupCallsign, user->getCallsign());	// inform QuadNet
			delete user;
			m_users.erase(it);
			// The iterator is now invalid, so we'll find the next expiry on the next clock tick with a
			// new iterator
			break;
		}
	}
}

void CGroupHandler::updateReflectorInfo()
{
	std::string subcommand("REFLECTOR");
	std::vector<std::string> parms;
	std::string callsign(m_groupCallsign);
	ReplaceChar(callsign, ' ', '_');
	parms.push_back(callsign);
	std::string reflector(m_linkReflector);
	if (reflector.size() < 8)
		reflector.assign("________");
	else
		ReplaceChar(reflector, ' ', '_');
	parms.push_back(reflector);
	switch (m_linkStatus) {
		case LS_LINKING_DCS:
		case LS_LINKING_DEXTRA:
		case LS_PENDING_IRCDDB:
			parms.push_back(std::string("LINKING"));
			break;
		case LS_LINKED_DCS:
		case LS_LINKED_DEXTRA:
			parms.push_back(std::string("LINKED"));
			break;
		case LS_NONE:
			parms.push_back(std::string("UNLINKED"));
			break;
		default:
			parms.push_back(std::string("FAILED"));
			break;
	}
	parms.push_back(std::to_string(m_userTimeout));
	std::string info(m_infoText);
	info.resize(20, '_');
	ReplaceChar(info, ' ', '_');
	parms.push_back(info);
	parms.push_back(std::string(m_listenOnly ? "1" : "0"));

	for (int i=0; i<2; i++) {
		if (m_irc[i])
			m_irc[i]->sendSGSInfo(subcommand, parms);
}	}


void CGroupHandler::logUser(LOGUSER lu, const std::string channel, const std::string user)
{
	std::string cmd(LU_OFF==lu ? "LOGOFF" : "LOGON");
	std::string chn(channel);
	std::string usr(user);
	ReplaceChar(chn, ' ', '_');
	ReplaceChar(usr, ' ', '_');
	std::vector<std::string> parms;
	parms.push_back(chn);
	parms.push_back(usr);
	for (int i=0; i<2; i++) {
		if (m_irc[i])
			m_irc[i]->sendSGSInfo(cmd, parms);
	}
}

void CGroupHandler::sendToRepeaters(CHeaderData& header) const
{
	for (auto it = m_repeaters.begin(); it != m_repeaters.end(); ++it) {
		CSGSRepeater *repeater = it->second;
		if (repeater != NULL) {
            //if (0 == repeater->m_repeater.find("N7TAE"))
			//    printf("sendToRepeater: rptr='%s' dest='%s' addr='%s' gate='%s'\n", repeater->m_repeater.c_str(), repeater->m_destination.c_str(), repeater->m_address.c_str(), repeater->m_gateway.c_str());
			const bool is_ipv4 = (std::string::npos == repeater->m_address.find(':'));
			int i = 0;
			if (is_ipv4 && m_irc[1])
				i = 1;
			header.setYourCall(repeater->m_destination);
			header.setDestination(repeater->m_address, is_ipv4 ? G2_DV_PORT : G2_IPV6_PORT);
			header.setRepeaters(repeater->m_gateway, repeater->m_repeater);
			m_g2Handler[i]->writeHeader(header);
		}
	}
}

void CGroupHandler::sendToRepeaters(CAMBEData &data) const
{
	for (auto it = m_repeaters.begin(); it != m_repeaters.end(); ++it) {
		CSGSRepeater *repeater = it->second;
		if (repeater != NULL) {
			const bool is_ipv4 = (std::string::npos == repeater->m_address.find(':'));
			int i = 0;
			if (is_ipv4 && m_irc[1])
				i = 1;
			data.setDestination(repeater->m_address, is_ipv4 ? G2_DV_PORT : G2_IPV6_PORT);
			m_g2Handler[i]->writeAMBE(data);
		}
	}
}

void CGroupHandler::sendFromText(const std::string &my) const
{
	std::string text;
	switch (m_callsignSwitch) {
		case SCS_GROUP_CALLSIGN:
			text = std::string("FROM %") + my;
			break;
		case SCS_USER_CALLSIGN:
			text = std::string("VIA SMARTGP ") + m_groupCallsign;
			break;
		default:
			break;
	}

	CSlowDataEncoder slowData;
	slowData.setTextData(text);

	CAMBEData data;
	data.setId(m_id);

	unsigned char buffer[DV_FRAME_LENGTH_BYTES];
	::memcpy(buffer + 0U, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);

	for (unsigned int i = 0U; i < 21U; i++) {
		if (i == 0U) {
			// The first AMBE packet is a sync
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES);
			data.setData(buffer, DV_FRAME_LENGTH_BYTES);
			data.setSeq(i);
		} else {
			// The packets containing the text data
			unsigned char slowDataBuffer[DATA_FRAME_LENGTH_BYTES];
			slowData.getTextData(slowDataBuffer);
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, slowDataBuffer, DATA_FRAME_LENGTH_BYTES);
			data.setData(buffer, DV_FRAME_LENGTH_BYTES);
			data.setSeq(i);
		}

		sendToRepeaters(data);
	}
}

void CGroupHandler::sendAck(const CUserData &user, const std::string &text) const
{
	unsigned int id = CHeaderData::createId();

	CHeaderData header(m_groupCallsign, "    ", user.getUser(), user.getGateway(), user.getRepeater());
	const bool is_ipv4 = (std::string::npos == user.getAddress().find(':'));
	header.setDestination(user.getAddress(), is_ipv4 ? G2_DV_PORT : G2_IPV6_PORT);
	const int index = (is_ipv4 && m_irc[1]) ? 1 : 0;
	header.setId(id);
	m_g2Handler[index]->writeHeader(header);

	CSlowDataEncoder slowData;
	slowData.setTextData(text);

    //printf("Sending ack user=%s rptr=%s gate=%s addr=%s index=%d\n", user.getUser().c_str(), user.getRepeater().c_str(), user.getGateway().c_str(), user.getAddress().c_str(), index);
	CAMBEData data;
	data.setId(id);
	data.setDestination(user.getAddress(), is_ipv4 ? G2_DV_PORT : G2_IPV6_PORT);

	unsigned char buffer[DV_FRAME_MAX_LENGTH_BYTES];
	::memcpy(buffer + 0U, NULL_AMBE_DATA_BYTES, VOICE_FRAME_LENGTH_BYTES);

	for (unsigned int i = 0U; i < 20U; i++) {
		if (i == 0U) {
			// The first AMBE packet is a sync
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, DATA_SYNC_BYTES, DATA_FRAME_LENGTH_BYTES);
			data.setData(buffer, DV_FRAME_LENGTH_BYTES);
			data.setSeq(i);
		} else if (i == 19U) {
			// The last packet of the ack
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, END_PATTERN_BYTES, END_PATTERN_LENGTH_BYTES);
			data.setData(buffer, DV_FRAME_MAX_LENGTH_BYTES);
			data.setSeq(i);
			data.setEnd(true);
		} else {
			// The packets containing the text data
			unsigned char slowDataBuffer[DATA_FRAME_LENGTH_BYTES];
			slowData.getTextData(slowDataBuffer);
			::memcpy(buffer + VOICE_FRAME_LENGTH_BYTES, slowDataBuffer, DATA_FRAME_LENGTH_BYTES);
			data.setData(buffer, DV_FRAME_LENGTH_BYTES);
			data.setSeq(i);
		}

		m_g2Handler[index]->writeAMBE(data);
	}
}

void CGroupHandler::linkUp(DSTAR_PROTOCOL, const std::string &callsign)
{
	printf("%s link to %s established\n", (LT_DEXTRA==m_linkType)?"DExtra":"DCS", callsign.c_str());

	m_linkStatus = (LT_DEXTRA == m_linkType) ? LS_LINKED_DEXTRA : LS_LINKED_DCS;
}

bool CGroupHandler::linkFailed(DSTAR_PROTOCOL, const std::string &callsign, bool isRecoverable)
{
	if (!isRecoverable) {
		if (m_linkStatus != LS_NONE) {
			printf("%s link to %s has failed\n", (LT_DEXTRA==m_linkType)?"DExtra":"DCS", callsign.c_str());
			m_linkStatus = LS_NONE;
		}

		return false;
	}

	if (m_linkStatus == LS_LINKING_DEXTRA || m_linkStatus == LS_LINKED_DEXTRA || m_linkStatus == LS_LINKING_DCS || m_linkStatus == LS_LINKED_DCS) {
		printf("%s link to %s has failed, relinking\n", (LT_DEXTRA==m_linkType)?"DExtra":"DCS", callsign.c_str());
		m_linkStatus = (LT_DEXTRA == m_linkType) ? LS_LINKING_DEXTRA : LS_LINKING_DCS;
		return true;
	}

	return false;
}

void CGroupHandler::linkRefused(DSTAR_PROTOCOL, const std::string &callsign)
{
	if (m_linkStatus != LS_NONE) {
		printf("%s link to %s was refused\n", (LT_DEXTRA==m_linkType)?"DExtra":"DCS", callsign.c_str());
		m_linkStatus = LS_NONE;
	}
}

bool CGroupHandler::singleHeader()
{
	return true;
}

DSTAR_LINKTYPE CGroupHandler::getLinkType()
{
	return m_linkType;
}

void CGroupHandler::setLinkType(DSTAR_LINKTYPE linkType)
{
	m_linkType = linkType;
}

void CGroupHandler::clearReflector()
{
	m_linkReflector.clear();
}
