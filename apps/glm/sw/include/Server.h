#pragma once

#include <vector>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <limits>

#include "FPGA_ColumnML.h"

using namespace std;

// #define PRINT_STATUS

static const struct timespec PAUSE {.tv_sec = 0, .tv_nsec = 1000};

enum FThreadState {idle, running, to_be_paused, paused, finished};

class FThread {
private:
	uint32_t m_id;
	FThreadState m_state;
	uint32_t m_numTimesResumed;
	double m_startTime;
	double m_stopTime;
	uint32_t m_priority;

public:
	FPGA_ColumnML* m_cML;

	FThread(FPGA_ColumnML* cML, uint32_t id, uint32_t priority) {
		m_state = idle;
		m_cML = cML;
		m_id = id;
		m_numTimesResumed = 0;
		m_startTime = get_time();
		m_stopTime = 0;
		m_priority = 0;
		m_cML-> ResetContext();
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
		auto output = m_cML->CastToInt('o');
		if (1 == output[0] && (output[4] & 0xFF) == 0) {
			m_state = finished;
			m_stopTime = get_time();
			return true;
		}
		return false;
	}

	bool IsPaused() {
		auto output = m_cML->CastToInt('o');
		if (1 == output[0] && (output[4] & 0xFF) > 0) {
			m_state = paused;
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
	vector<FThread*> m_runningThreads;

public:

	Instance(uint32_t id) {
		m_id = id;
	}

	uint32_t GetId() {
		return m_id;
	}

	bool operator< (Instance &other) {
		return GetNumThreads() < other.GetNumThreads();
	}

	void AddThread(FThread* fthread) {
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
		for (FThread* t: m_runningThreads) {
			if (t->IsFinished()) {
				cout << "------ FThread with id: " << t->GetId() << " is finished" << endl;
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
				if (t->GetState() == running) {
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
		uint32_t pos = -1;
		for (FThread* t: m_runningThreads) {
			uint32_t minimum = numeric_limits<uint32_t>::max();
			if ((t->GetState() == idle || t->GetState() == paused) && t->GetNumTimesResumed() < minimum) {
				minimum = t->GetNumTimesResumed();
				threadToMigrate = t;
				pos = i;
			}
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
	static const uint32_t MAX_MEMORY_SIZE = (1 << 10)*16;
	static const uint32_t NUM_INSTANCES = 2;
	static const uint32_t vc_select = 0;

	bool m_run;
	mutex m_mtx;
	condition_variable m_cv;

	thread m_serverThread;
	queue<FThread*> m_requestQueue;
	Instance* m_instance[NUM_INSTANCES];
	uint32_t m_numThreads;
	bool m_enableContextSwitch;
	bool m_enableThreadMigration;

	void ResumeThread(FThread* fthread, uint32_t whichInstance) {
		if (fthread == NULL) {
			return;
		}

#ifdef PRINT_STATUS
		cout << "ResumeThread with id: " << fthread->GetId() << endl;
#endif
		fthread->Resume();

		auto programMemory = fthread->m_cML->CastToInt('p');
		auto inputMemory = fthread->m_cML->CastToInt('i');
		auto outputMemory = fthread->m_cML->CastToInt('o');

		outputMemory[0] = 0;
		iFPGA::writeCSR(whichInstance*4 + 0, (vc_select << 30) | (fthread->m_cML->m_numInstructions & 0xFF) );
		iFPGA::writeCSR(whichInstance*4 + 1, intptr_t(programMemory));
		iFPGA::writeCSR(whichInstance*4 + 2, intptr_t(inputMemory));
		iFPGA::writeCSR(whichInstance*4 + 3, intptr_t(outputMemory));
	}

	void PauseThread(FThread* fthread, uint32_t whichInstance) {
		if (fthread == NULL) {
			return;
		}

#ifdef PRINT_STATUS
		cout << "PauseThread with id: " << fthread->GetId() << endl;
#endif
		iFPGA::writeCSR(whichInstance*4 + 0, (vc_select << 30) | (1 << 16) | (fthread->m_cML->m_numInstructions & 0xFF) );
		fthread->Pause();
	}

	void ScheduleThreads() {
		for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
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
		for (uint32_t i = 0; i < NUM_INSTANCES; i++) {
			temp.push_back(m_instance[i]);
		}
		sort(temp.begin(), temp.end());

		if (temp.front()->GetNumThreads() < temp.back()->GetNumThreads()-1) {
			FThread* fthread = temp.back()->GetThreadToMigrate();
			if (fthread != NULL) {
				cout << "Migrate thread with id: " << fthread->GetId() << endl;
				cout << "temp.front()->GetNumThreads(): " << temp.front()->GetNumThreads() << endl;
				cout << "temp.back()->GetNumThreads(): " << temp.back()->GetNumThreads() << endl;
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
					uint32_t whichInstance = -1;
					uint32_t minimumLoad = numeric_limits<uint32_t>::max();
					for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
						if (m_instance[k]->GetNumThreads() < minimumLoad) {
							minimumLoad = m_instance[k]->GetNumThreads();
							whichInstance = k;
						}
					}

					if (whichInstance != -1) {
						cout << "Putting fthread " << fthread->GetId() << " to instance " << whichInstance << endl;
						m_instance[whichInstance]->AddThread(fthread);
						m_requestQueue.pop();
					}
				}
				else {
					cout << "Request is nullptr" << endl;
					m_requestQueue.pop();
				}
			}
			if (NUM_INSTANCES > 1 && m_enableThreadMigration) {
				RedistributeThreads();
			}
			ScheduleThreads();
			lck.unlock();

			nanosleep(&PAUSE, NULL);

			thereAreRunningThreads = false;
			for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
				m_instance[k]->UpdateStates();
				if (m_instance[k]->GetNumThreads() > 0) {
					thereAreRunningThreads = true;
				}
			}

			i++;
			if (i == 50000) {
				cout << "------------------------ Running threads: " << endl;
				for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
					cout << "---- On instance: " << k << endl;
					m_instance[k]->PrintStatus();
				}
				i = 0;
			}
		}

		cout << "Finishing server thread..." << endl;
	}

public:
	Server(const char* accel_uuid, bool enableContextSwitch, bool enableThreadMigration) : iFPGA(accel_uuid) {
		unique_lock<mutex> lck(m_mtx);

		m_enableContextSwitch = enableContextSwitch;
		m_enableThreadMigration = enableThreadMigration;

		m_numThreads = 0;
		for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
			m_instance[k] = new Instance(k);
		}
		m_serverThread = thread(&Server::ProcessRequests, this);
		cout << "Waiting..." << endl;
		m_cv.wait(lck);
	}

	~Server() {
		cout << "Deleting..." << endl;
		for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
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
		cout << "Push" << endl;
		FThread* fthread = new FThread(cML, m_numThreads++, 0);
		m_requestQueue.push(fthread);
		return fthread;
	}

	FThread* Request(FPGA_ColumnML* cML, uint32_t priority) {
		unique_lock<mutex> lck(m_mtx);
		cout << "Push" << endl;
		FThread* fthread = new FThread(cML, m_numThreads++, priority);
		m_requestQueue.push(fthread);
		return fthread;
	}
};
 