#pragma once

#include <vector>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <limits>

#include "FPGA_ColumnML.h"

using namespace std;

static const struct timespec PAUSE {.tv_sec = 0, .tv_nsec = 1000};

enum FThreadState {idle, running, paused, finished};

class FThread {
private:
	uint32_t m_id;

public:
	FPGA_ColumnML* m_cML;
	FThreadState m_state;

	FThread(FPGA_ColumnML* cML, uint32_t id) {
		m_state = idle;
		m_cML = cML;
		m_id = id;
	}

	uint32_t GetId() {
		return m_id;
	}

	void Resume() {
		m_state = running;
	}

	void Pause() {
		m_state = paused;
	}

	void Finish() {
		m_state = finished;
	}

	string GetState() {
		switch(m_state) {
			case idle:
				return "idle";
			case running:
				return "running";
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

class Server : public iFPGA {
private:
	static const uint32_t MAX_MEMORY_SIZE = (1 << 10)*16;
	static const uint32_t NUM_INSTANCES = 1;
	static const uint32_t vc_select = 0;

	bool m_run;
	mutex m_mtx;
	condition_variable m_cv;

	thread m_serverThread;
	queue<FThread*> m_requestQueue;
	vector<FThread*> m_runningThreads[NUM_INSTANCES];
	uint32_t m_numThreads;

	void ResumeThread(FThread* thread, uint32_t whichInstance) {
		if (thread == NULL) {
			return;
		}

		cout << "ResumeThread with id: " << thread->GetId() << endl;
		thread->Resume();

		auto programMemory = thread->m_cML->CastToInt('p');
		auto inputMemory = thread->m_cML->CastToInt('i');
		auto outputMemory = thread->m_cML->CastToInt('o');

		outputMemory[0] = 0;
		iFPGA::writeCSR(whichInstance*4 + 0, (vc_select << 30) | (thread->m_cML->m_numInstructions & 0xFF) );
		iFPGA::writeCSR(whichInstance*4 + 1, intptr_t(programMemory));
		iFPGA::writeCSR(whichInstance*4 + 2, intptr_t(inputMemory));
		iFPGA::writeCSR(whichInstance*4 + 3, intptr_t(outputMemory));
	}

	void PauseThread(FThread* thread, uint32_t whichInstance) {
		if (thread == NULL) {
			return;
		}

		cout << "PauseThread with id: " << thread->GetId() << endl;
		iFPGA::writeCSR(whichInstance*4 + 0, (vc_select << 30) | (1 << 16) | (thread->m_cML->m_numInstructions & 0xFF) );

		auto output = thread->m_cML->CastToInt('o');
		while (0 == output[0]) {
			nanosleep(&PAUSE, NULL);
		}
		if ((output[4] & 0xFF) == 0) { // Check if pc is 0
			thread->Finish();
		}
		else {
			thread->Pause();
		}
	}

	void ScheduleThreads() {

		FThread* threadToPause = NULL;
		FThread* threadToResume = NULL;

		for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
			for (FThread* t: m_runningThreads[k]) {
				if (t->m_state == idle || t->m_state == paused) {
					threadToResume = t;
					break;
				}
			}
			for (FThread* t: m_runningThreads[k]) {
				if (t->m_state == running) {
					threadToPause = t;
				}
			}

			PauseThread(threadToPause, k);
			ResumeThread(threadToResume, k);
		}
	}

	void ProcessRequests() {
		unique_lock<mutex> lck(m_mtx, defer_lock);

		m_run = true;
		cout << "Start server thread..." << endl;
		bool thereAreRunningThreads = true;
		m_cv.notify_one();

		uint32_t i = 0;
		while(m_run || !m_requestQueue.empty() || thereAreRunningThreads) {

			lck.lock();
			if (!m_requestQueue.empty()) {
				FThread* thread = m_requestQueue.front();

				if ( thread != nullptr ) {
					uint32_t whichInstance = -1;
					uint32_t minimumLoad = numeric_limits<uint32_t>::max();
					for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
						if (m_runningThreads[k].size() < minimumLoad) {
							minimumLoad = m_runningThreads[k].size();
							whichInstance = k;
						}
					}

					if (whichInstance != -1) {
						m_runningThreads[whichInstance].push_back(thread);
						m_requestQueue.pop();
					}
				}
				else {
					cout << "Request is nullptr" << endl;
					m_requestQueue.pop();
				}
			}
			ScheduleThreads();
			lck.unlock();

			nanosleep(&PAUSE, NULL);

			thereAreRunningThreads = false;
			for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
				vector<uint32_t> toErase;
				uint32_t pos = 0;
				for (FThread* t: m_runningThreads[k]) {
					if (t->m_state == finished) {
						cout << "------ FThread with id: " << t->GetId() << " is finished" << endl;
						toErase.push_back(pos);
					}
					else {
						thereAreRunningThreads = true;
					}
					pos++;
				}
				for (uint32_t p: toErase) {
					m_runningThreads[k].erase(m_runningThreads[k].begin() + p);
				}
			}

			i++;
			if (i == 50000) {
				cout << "------------------------ Running threads: " << endl;
				for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
					cout << "---- On instance: " << k << endl;
					for (FThread* t: m_runningThreads[k]) {
						cout << "------ FThread with id: " << t->GetId();
						cout << ", state: " << t->GetState() << endl;
					}
				}
				i = 0;
			}
		}

		cout << "Finishing server thread..." << endl;
	}

public:
	Server(const char* accel_uuid) : iFPGA(accel_uuid) {
		unique_lock<mutex> lck(m_mtx);
		m_numThreads = 0;
		m_serverThread = thread(&Server::ProcessRequests, this);
		cout << "Waiting..." << endl;
		m_cv.wait(lck);
	}

	~Server() {
		cout << "Deleting..." << endl;
		m_run = false;
		m_serverThread.join();
	}

	FThread* Request(FPGA_ColumnML* cML) {
		unique_lock<mutex> lck(m_mtx);
		cout << "Push" << endl;
		FThread* thread = new FThread(cML, m_numThreads++);
		m_requestQueue.push(thread);
		return thread;
	}
};
 