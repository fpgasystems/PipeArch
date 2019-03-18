#pragma once

#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

#include "FPGA_ColumnML.h"

using namespace std;

class Server {
private:
	thread m_serverThread;
	queue<ColumnML*> m_requestQueue;

	bool m_run;
	mutex m_mtx;
	condition_variable m_cv;

	void ProcessRequests() {
		unique_lock<mutex> lck(m_mtx);

		m_run = true;
		cout << "Start server thread..." << endl;
		m_cv.notify_one();

		while(m_run) {
			m_cv.wait(lck);
			if (!m_requestQueue.empty()) {
				while (!m_requestQueue.empty()) {
					cout << "Processing request" << endl;
					ColumnML* cML = m_requestQueue.front();


					



					m_requestQueue.pop();
				}
			}
		}

		cout << "Finishing server thread..." << endl;
	}

public:
	Server() {
		unique_lock<mutex> lck(m_mtx);
		m_serverThread = thread(&Server::ProcessRequests, this);
		cout << "Waiting..." << endl;
		m_cv.wait(lck);
	}

	~Server() {
		unique_lock<mutex> lck(m_mtx);
		m_run = false;
		m_cv.notify_one();
		lck.unlock();
		cout << "Deleting..." << endl;
		m_serverThread.join();
	}

	void Request(ColumnML* cML) {
		unique_lock<mutex> lck(m_mtx);
		cout << "Push" << endl;
		m_requestQueue.push(cML);
		m_cv.notify_one();
	}
};
 