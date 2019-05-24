#pragma once

#include <iostream> 
#include <algorithm>
#include <vector>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <limits>

#include "FPGA_ColumnML.h"

using namespace std;

// #define PRINT_STATUS
// #define VERBOSE

static const struct timespec PAUSE {.tv_sec = 0, .tv_nsec = 1000};

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
	FPGA_ColumnML* m_cML;

	FThread(FPGA_ColumnML* cML, uint32_t id, uint32_t priority) {
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
		if (1 == output[0] && (output[4] & 0xFF) == 0) {
			m_state = finished;
			m_stopTime = get_time();
			m_outputCopyRequested = false;
			return true;
		}
		return false;
	}

	bool IsPaused() {
		auto output = iFPGA::CastToInt(m_cML->m_outputHandle);
		if (1 == output[0] && (output[4] & 0xFF) > 0) {
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

	uint32_t GetId() {
		return m_id;
	}

	static bool CompareNumThreads (Instance* first, Instance* second) {
		return first->GetNumThreads() < second->GetNumThreads();
	}

	void AddThread(FThread* fthread) {
#ifdef XILINX
		// m_ifpga->UpdateBank(fthread->m_cML->m_inputHandle, m_id);
		// m_ifpga->UpdateBank(fthread->m_cML->m_outputHandle, m_id);
		// m_ifpga->UpdateBank(fthread->m_cML->m_programMemoryHandle, m_id);
		vector<cl::Memory> buffersToCopy;
		// buffersToCopy.push_back(iFPGA::CastToPtr(fthread->m_cML->m_inputHandle));
		buffersToCopy.push_back(iFPGA::CastToPtr(fthread->m_cML->m_outputHandle));
		buffersToCopy.push_back(iFPGA::CastToPtr(fthread->m_cML->m_programMemoryHandle));
		m_ifpga->CopyToFPGA(buffersToCopy);
#endif
		m_runningThreads.push_back(fthread);
	}

	uint32_t GetNumThreads() {
		return m_runningThreads.size();
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

	void UpdateStates() {
		vector<uint32_t> toErase;
		uint32_t pos = 0;

#ifdef XILINX
		vector<cl::Memory> buffersToCopy;
		for (FThread* t: m_runningThreads) {
			if (t->m_outputCopyRequested == false && (t->GetState() == running || t->GetState() == to_be_paused) /*&& m_ifpga->GetQueueCount(m_id) == 0*/) {
				buffersToCopy.push_back(m_ifpga->CastToPtr(t->m_cML->m_outputHandle));
				t->m_outputCopyRequested = true;
			}
		}
		if (buffersToCopy.size() > 0) {
			m_ifpga->CopyFromFPGA(buffersToCopy, m_id);
		}
#endif

		for (FThread* t: m_runningThreads) {
			if (t->IsFinished()) {
#ifdef VERBOSE
				cout << "------ FThread with id: " << t->GetId() << " is finished" << endl;
#endif
				toErase.push_back(pos);
			}
			t->IsPaused();
			pos++;
		}
		for (uint32_t p: toErase) {
			m_runningThreads.erase(m_runningThreads.begin() + p);
		}
	}

	void PrintStatus() {
		for (FThread* t: m_runningThreads) {
			cout << "------ FThread with id: " << t->GetId();
			cout << ", state: " << t->GetStateString() << endl;
		}
	}

	FThread* GetThreadToPause() {
		FThread* threadToPause = NULL;

		if (GetNumWaitingThreads() > 0) {
			for (FThread* t: m_runningThreads) {
				if (t->GetState() == running && t->GetPriority() == 0) {
					threadToPause = t;
				}
			}
		}

		return threadToPause;
	}

	FThread* GetThreadToResume() {
		FThread* threadToResume = NULL;

		uint32_t minimum = numeric_limits<uint32_t>::max();
		for (FThread* t: m_runningThreads) {
			if (t->GetState() == idle || t->GetState() == paused) {

				if (t->GetPriority() > 0) {
					threadToResume = t;
					return threadToResume;
				}

				if (t->GetNumTimesResumed() < minimum) {
					minimum = t->GetNumTimesResumed();
					threadToResume = t;
				}

			}
		}

		return threadToResume;
	}

	FThread* GetThreadToMigrate() {
		FThread* threadToMigrate = NULL;

		uint32_t i = 0;
		int pos = -1;
		for (FThread* t: m_runningThreads) {
			uint32_t minimum = numeric_limits<uint32_t>::max();
#ifdef VERBOSE
			cout << "t->GetNumTimesResumed(): " << t->GetNumTimesResumed() << endl;
#endif
			if ((t->GetState() == idle || t->GetState() == paused) && t->GetNumTimesResumed() < minimum) {
				minimum = t->GetNumTimesResumed();
				threadToMigrate = t;
				pos = i;
			}
#ifdef VERBOSE
			cout << "pos: " << pos << endl;
#endif
			i++;
		}

		if (pos != -1) {
			m_runningThreads.erase(m_runningThreads.begin()+pos);
		}

		return threadToMigrate;
	}

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

	void ResumeThread(FThread* fthread, uint32_t whichInstance) {
		if (fthread == NULL) {
			return;
		}
#ifdef VERBOSE
		if (fthread->GetPriority() > 0) {
			cout << "-----------------ResumeThread with id: " << fthread->GetId() << endl;
		}
#endif

#ifdef PRINT_STATUS
		cout << "ResumeThread with id: " << fthread->GetId() << endl;
#endif
		fthread->Resume();

		auto output = iFPGA::CastToInt(fthread->m_cML->m_outputHandle);
		output[0] = 0;
#ifdef VERBOSE
		cout << "output resetted" << endl;
#endif
		auto programMemory = iFPGA::CastToPtr(fthread->m_cML->m_programMemoryHandle);
		auto inputMemory = iFPGA::CastToPtr(fthread->m_cML->m_inputHandle);
		auto outputMemory = iFPGA::CastToPtr(fthread->m_cML->m_outputHandle);
#ifdef VERBOSE
		cout << "Writing args" << endl;
#endif

#ifdef XILINX // On sdaccel we have to trigger context switch already while starting the kernel
		uint64_t triggerContextSwitch = m_enableContextSwitch;
		iFPGA::WriteConfigReg(0, (vc_select << 30) | (triggerContextSwitch << 16) | (fthread->m_cML->m_numInstructions & 0xFF) );
		iFPGA::WriteConfigReg(1, programMemory);
		iFPGA::WriteConfigReg(2, inputMemory);
		iFPGA::WriteConfigReg(3, outputMemory);
		iFPGA::StartKernel(whichInstance);
#else // On opae we can write the config registers dynamically to trigger context switch only when necessary
		uint64_t triggerContextSwitch = 0;
		iFPGA::WriteConfigReg(whichInstance*4 + 0, (vc_select << 30) | (triggerContextSwitch << 16) | (fthread->m_cML->m_numInstructions & 0xFF) );
		iFPGA::WriteConfigReg(whichInstance*4 + 1, programMemory);
		iFPGA::WriteConfigReg(whichInstance*4 + 2, inputMemory);
		iFPGA::WriteConfigReg(whichInstance*4 + 3, outputMemory);
#endif
	}

	void PauseThread(FThread* fthread, uint32_t whichInstance) {
		if (fthread == NULL) {
			return;
		}

#ifdef VERBOSE
		if (fthread->GetPriority() > 0) {
			cout << "-----------------PauseThread with id: " << fthread->GetId() << endl;
		}
#endif

#ifdef PRINT_STATUS
		cout << "PauseThread with id: " << fthread->GetId() << endl;
#endif
#ifndef XILINX // We cannot write config registers while a kernel is running in sdaccel
		iFPGA::WriteConfigReg(whichInstance*4 + 0, (vc_select << 30) | (1 << 16) | (fthread->m_cML->m_numInstructions & 0xFF) );
#endif
		fthread->Pause();
	}

	void ScheduleThreads() {
		for (uint32_t k = 0; k < m_numInstances; k++) {
			FThread* threadToPause = m_instance[k]->GetThreadToPause();
			FThread* threadToResume = m_instance[k]->GetThreadToResume();

			if (m_instance[k]->GetNumRunningThreads() == 0) {
				ResumeThread(threadToResume, k);
			}
			if (m_enableContextSwitch) {
				PauseThread(threadToPause, k);
			}
		}
	}

	void RedistributeThreads() {
		vector<Instance*> temp;
		for (uint32_t i = 0; i < m_numInstances; i++) {
			temp.push_back(m_instance[i]);
		}
		stable_sort(temp.begin(), temp.end(), Instance::CompareNumThreads);

		if (temp.front()->GetNumThreads() < temp.back()->GetNumThreads()-1) {
			FThread* fthread = temp.back()->GetThreadToMigrate();
			if (fthread != NULL) {
#ifdef VERBOSE
				cout << "Migrate thread with id: " << fthread->GetId() << endl;
				cout << "temp.front()->GetNumThreads(): " << temp.front()->GetNumThreads() << endl;
				cout << "temp.back()->GetNumThreads(): " << temp.back()->GetNumThreads() << endl;
#endif
				temp.front()->AddThread(fthread);
			}
		}
	}

	void ProcessRequests() {
		unique_lock<mutex> lck(m_mtx);

		m_run = true;
		cout << "Start server thread..." << endl;
		bool thereAreRunningThreads = true;
		m_cv.notify_one();
		lck.unlock();

		uint32_t i = 0;
		while(m_run || !m_requestQueue.empty() || thereAreRunningThreads) {

			lck.lock();
			if (!m_requestQueue.empty()) {
				FThread* fthread = m_requestQueue.front();

				if ( fthread != nullptr ) {
					int whichInstance = -1;
					uint32_t minimumLoad = numeric_limits<uint32_t>::max();
					for (uint32_t k = 0; k < m_numInstances; k++) {
						if (m_instance[k]->GetNumThreads() < minimumLoad) {
							minimumLoad = m_instance[k]->GetNumThreads();
							whichInstance = k;
						}
					}

					if (whichInstance != -1) {
#ifdef VERBOSE
						cout << "Putting fthread " << fthread->GetId() << " to instance " << whichInstance << endl;
#endif
						m_instance[whichInstance]->AddThread(fthread);
						m_requestQueue.pop();
					}
				}
				else {
					cout << "Request is nullptr" << endl;
					m_requestQueue.pop();
				}
			}
#ifndef XILINX
			if (m_numInstances > 1 && m_enableThreadMigration) {
				RedistributeThreads();
			}
#endif
			ScheduleThreads();
			lck.unlock();

			nanosleep(&PAUSE, NULL);

			thereAreRunningThreads = false;
			for (uint32_t k = 0; k < m_numInstances; k++) {
				m_instance[k]->UpdateStates();
				if (m_instance[k]->GetNumThreads() > 0) {
					thereAreRunningThreads = true;
				}
			}

#ifdef PRINT_STATUS
			i++;
			if (i == 50000) {
				cout << "------------------------ Running threads: " << endl;
				for (uint32_t k = 0; k < m_numInstances; k++) {
					cout << "---- On instance: " << k << endl;
					m_instance[k]->PrintStatus();
				}
				i = 0;
			}
#endif
		}

		cout << "Finishing server thread..." << endl;
	}

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
		bool enableThreadMigration) : iFPGA(iFPGA::MAX_NUM_INSTANCES)
	{
		m_enableContextSwitch = enableContextSwitch;
		m_enableThreadMigration = enableThreadMigration;

		ConstructorCommon();
	}

	Server(bool enableContextSwitch,
		bool enableThreadMigration,
		uint32_t numInstances) : iFPGA(numInstances)
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

	FThread* Request(FPGA_ColumnML* cML) {
		unique_lock<mutex> lck(m_mtx);
		// cout << "Push" << endl;
		FThread* fthread = new FThread(cML, m_numThreads++, 0);
		m_requestQueue.push(fthread);
		return fthread;
	}

	FThread* Request(FPGA_ColumnML* cML, uint32_t priority) {
		unique_lock<mutex> lck(m_mtx);
		// cout << "Push" << endl;
		FThread* fthread = new FThread(cML, m_numThreads++, priority);
		m_requestQueue.push(fthread);
		return fthread;
	}
};
 