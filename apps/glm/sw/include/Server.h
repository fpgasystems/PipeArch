// Copyright (C) 2018 Kaan Kara - Systems Group, ETH Zurich

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//*************************************************************************

#pragma once

#include <iostream> 
#include <algorithm>
#include <vector>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <limits>

#include "FPGA_Program.h"

using namespace std;

#define SERVER_PRINT_STATUS
#define SERVER_VERBOSE

static const struct timespec PAUSE {.tv_sec = 0, .tv_nsec = 1000};
static const struct timespec MSPAUSE {.tv_sec = 0, .tv_nsec = 1000000};

enum FThreadState {idle, running, to_be_paused, paused, finished};

class FThread {
private:
	FThreadState m_state;
	uint32_t m_id;
	uint32_t m_numTimesResumed;
	double m_startTime;
	double m_stopTime;
	uint32_t m_priority;

public:
	bool m_outputCopyRequested;
	FPGA_Program* m_cML;

	FThread(FPGA_Program* cML, uint32_t id, uint32_t priority) {
		m_state = idle;
		m_cML = cML;
		m_id = id;
		m_numTimesResumed = 0;
		m_startTime = get_time();
		m_stopTime = 0;
		m_priority = priority;
		m_outputCopyRequested = false;
		m_cML->ResetContext();
	}

	uint32_t GetId() {
		return m_id;
	}

	double GetResponseTime() {
		return m_stopTime - m_startTime;
	}

	uint32_t GetNumTimesResumed() {
		return m_numTimesResumed;
	}

	uint32_t GetPriority() {
		return m_priority;
	}

	void Resume() {
		m_numTimesResumed++;
		m_state = running;
	}

	void Pause() {
		m_state = to_be_paused;
	}

	FThreadState GetState() {
		return m_state;
	}

	bool IsFinished() {
		auto output = iFPGA::CastToInt(m_cML->m_outputHandle);
		if (1 == output[0] && (output[6] & 0xFF) == 0) { // output[6]&0xFF is pc
			m_state = finished;
			m_stopTime = get_time();
			m_outputCopyRequested = false;
			return true;
		}
		return false;
	}

	bool IsPaused() {
		auto output = iFPGA::CastToInt(m_cML->m_outputHandle);
		if (1 == output[0] && (output[6] & 0xFF) > 0) { // output[6]&0xFF is pc
			output[4] = output[6]; // We added reg[3] and reg[4] later on... Need to fix this in HW.
			m_state = paused;
			m_outputCopyRequested = false;
			return true;
		}
		return false;
	}

	string GetStateString() {
		switch(m_state) {
			case idle:
				return "idle";
			case running:
				return "running";
			case to_be_paused:
				return "to_be_paused";
			case paused:
				return "paused";
			case finished:
				return "finished";
		}
		return "error";
	}

	void WaitUntilFinished() {
		while(m_state != finished) {
			nanosleep(&PAUSE, NULL);
		}
	}
};

class Instance {
private:
	uint32_t m_id;
	iFPGA* m_ifpga;
	vector<FThread*> m_runningThreads;

public:

	Instance(uint32_t id, iFPGA* ifpga) {
		m_id = id;
		m_ifpga = ifpga;
	}

	void PrintStatus() {
		for (FThread* t: m_runningThreads) {
			cout << "------ FThread with id: " << t->GetId();
			cout << ", state: " << t->GetStateString() << endl;
		}
	}

	uint32_t GetId() {
		return m_id;
	}

	static bool CompareNumThreads (Instance* first, Instance* second) {
		return first->GetNumThreads() < second->GetNumThreads();
	}

	uint32_t GetNumWaitingThreads() {
		uint32_t result = 0;
		for (FThread* t: m_runningThreads) {
			result += (t->GetState() == idle || t->GetState() == paused) ? 1 : 0;
		}
		return result;
	}

	uint32_t GetNumRunningThreads() {
		uint32_t result = 0;
		for (FThread* t: m_runningThreads) {
			result += (t->GetState() == running || t->GetState() == to_be_paused) ? 1 : 0;
		}
		return result;
	}

	uint32_t GetNumThreads() {
		return m_runningThreads.size();
	}

	void AddThread(FThread* fthread);
	void UpdateStates();
	FThread* GetThreadToPause();
	FThread* GetThreadToResume();
	FThread* GetThreadToMigrate();
};

class Server : public iFPGA {
private:
	static const uint32_t vc_select = 0;

	bool m_run;
	mutex m_mtx;
	condition_variable m_cv;

	thread m_serverThread;
	queue<FThread*> m_requestQueue;
	Instance* m_instance[iFPGA::MAX_NUM_INSTANCES];
	uint32_t m_numThreads;
	bool m_enableContextSwitch;
	bool m_enableThreadMigration;

	void ResumeThread(FThread* fthread, uint32_t whichInstance);
	void PauseThread(FThread* fthread, uint32_t whichInstance);
	void ScheduleThreads();
	void RedistributeThreads();
	void ProcessRequests();

	inline void ConstructorCommon() {
		unique_lock<mutex> lck(m_mtx);
		m_numThreads = 0;
		for (uint32_t k = 0; k < m_numInstances; k++) {
			m_instance[k] = new Instance(k, this);
		}
		m_serverThread = thread(&Server::ProcessRequests, this);
		cout << "Waiting..." << endl;
		m_cv.wait(lck);
	}

public:
	Server(bool enableContextSwitch,
		bool enableThreadMigration,
		uint32_t whichBank,
		xDevice* xdevice) : iFPGA(iFPGA::MAX_NUM_INSTANCES, whichBank, xdevice)
	{
		m_enableContextSwitch = enableContextSwitch;
		m_enableThreadMigration = enableThreadMigration;

		ConstructorCommon();
	}

	Server(bool enableContextSwitch,
		bool enableThreadMigration,
		uint32_t numInstances,
		uint32_t whichBank,
		xDevice* xdevice) : iFPGA(numInstances, whichBank, xdevice)
	{
		m_enableContextSwitch = enableContextSwitch;
		m_enableThreadMigration = enableThreadMigration;

		ConstructorCommon();
	}

	~Server() {
		cout << "Deleting..." << endl;
		for (uint32_t k = 0; k < m_numInstances; k++) {
			delete m_instance[k];
		}
		m_run = false;
		m_serverThread.join();
	}

	void ResetNumThreads() {
		m_numThreads = 0;
	}

	FThread* Request(FPGA_Program* cML) {
		PreCopy(cML);
		unique_lock<mutex> lck(m_mtx);
		FThread* fthread = new FThread(cML, m_numThreads++, 0);
		m_requestQueue.push(fthread);
		return fthread;
	}

	FThread* Request(FPGA_Program* cML, uint32_t priority) {
		PreCopy(cML);
		unique_lock<mutex> lck(m_mtx);
		FThread* fthread = new FThread(cML, m_numThreads++, priority);
		m_requestQueue.push(fthread);
		return fthread;
	}

	void PreCopy(FPGA_Program* cML) {
#ifdef XILINX
		while(!DoesInputHandleFitDRAM(cML->m_inputHandle, cML->GetNumCLsAllocated())) {
			nanosleep(&MSPAUSE, NULL);
		}
		if( HandleNeedsCopying(cML->m_inputHandle) ) {
			CopyInputHandleToFPGA(cML->m_inputHandle, cML->GetNumCLsAllocated());
		}
#endif
	}

	void GetInputHandleFromFPGA(FPGA_Program* cML) {
#ifdef XILINX
		CopyFromFPGASingle(cML->m_inputHandle, 0);
#endif
	}
};
 