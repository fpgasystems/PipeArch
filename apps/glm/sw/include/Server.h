#pragma once

#include <vector>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

#include "FPGA_ColumnML.h"

using namespace std;

class FThread {
private:
	uint32_t m_id;

public:
	FPGA_ColumnML* m_cML;

	FThread(FPGA_ColumnML* cML, uint32_t id) {
		m_cML = cML;
		m_id = id;
	}

	uint32_t GetId() {
		return m_id;
	}
};

class Server : public iFPGA {
private:
	static const uint32_t MAX_MEMORY_SIZE = (1 << 10)*16;
	static const uint32_t NUM_INSTANCES = 2;

	bool m_run;
	mutex m_mtx;
	condition_variable m_cv;

	thread m_serverThread;
	queue<FPGA_ColumnML*> m_requestQueue;
	vector<FThread*> m_runningThreads[NUM_INSTANCES];
	uint32_t m_numThreads;

	void StartProgram(FPGA_ColumnML* cML, uint32_t whichInstance) {

		FThread* newThread = new FThread(cML, m_numThreads++);
		m_runningThreads[whichInstance].push_back(newThread);

		auto programMemory = cML->CastToInt('p');
		auto inputMemory = cML->CastToInt('i');
		auto outputMemory = cML->CastToInt('o');

		outputMemory[0] = 0;
		uint32_t whichThread = 0;
		uint32_t vc_select = 0;
		iFPGA::writeCSR(whichInstance*4 + 0, (vc_select << 30) | (whichThread << 16) | (cML->m_numInstructions & 0xFFFF) );
		iFPGA::writeCSR(whichInstance*4 + 1, intptr_t(programMemory));
		iFPGA::writeCSR(whichInstance*4 + 2, intptr_t(inputMemory));
		iFPGA::writeCSR(whichInstance*4 + 3, intptr_t(outputMemory));
	}

	void ProcessRequests() {
		unique_lock<mutex> lck(m_mtx, defer_lock);

		struct timespec pause;
		pause.tv_sec = 0;
		pause.tv_nsec = 1000;

		m_run = true;
		cout << "Start server thread..." << endl;
		bool thereAreRunningThreads = true;
		m_cv.notify_one();

		uint32_t i = 0;
		while(m_run || !m_requestQueue.empty() || thereAreRunningThreads) {

			lck.lock();
			if (!m_requestQueue.empty()) {
				FPGA_ColumnML* cML = m_requestQueue.front();

				if ( cML != nullptr ) {

					int foundEmpty = -1;
					for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
						if (m_runningThreads[k].empty()) {
							foundEmpty = k;
							break;
						}
					}

					if (foundEmpty != -1) {
						cout << "Processing request" << endl;
						StartProgram(cML, foundEmpty);
						m_requestQueue.pop();
					}
				}
				else {
					cout << "Request is nullptr" << endl;
					m_requestQueue.pop();
				}

			}
			lck.unlock();

			nanosleep(&pause, NULL);

			thereAreRunningThreads = false;
			for (uint32_t k = 0; k < NUM_INSTANCES; k++) {
				vector<uint32_t> toErase;
				uint32_t pos = 0;
				for (FThread* t: m_runningThreads[k]) {
					auto output = t->m_cML->CastToFloat('o');
					if (output[0] != 0) {
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
						cout << "------ FThread with id: " << t->GetId() << endl;
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

	void Request(FPGA_ColumnML* cML) {
		unique_lock<mutex> lck(m_mtx);
		cout << "Push" << endl;
		m_requestQueue.push(cML);
	}
};
 