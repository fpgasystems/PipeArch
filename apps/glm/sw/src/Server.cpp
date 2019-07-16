#include "Server.h"

void Instance::AddThread(FThread* fthread) {
#ifdef XILINX
	vector<cl::Memory> buffersToCopy;
	buffersToCopy.push_back(iFPGA::CastToPtr(fthread->m_cML->m_outputHandle));
	buffersToCopy.push_back(iFPGA::CastToPtr(fthread->m_cML->m_programMemoryHandle));
	m_ifpga->CopyToFPGA(buffersToCopy);
#endif
	m_runningThreads.push_back(fthread);
}

void Instance::UpdateStates() {
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
#ifdef SERVER_VERBOSE
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

FThread* Instance::GetThreadToPause() {
	FThread* threadToPause = NULL;

	if (GetNumWaitingThreads() > 0) {
		uint32_t maxPriority = GetHighestPriority();
		for (FThread* t: m_runningThreads) {
			if (t->GetState() == running) { // There is always one running thread
				if (m_enablePriority && t->GetPriority() < maxPriority) { // ShortestJobFirst
					threadToPause = t;
				}
				else if (!m_enablePriority) { // RoundRobin
					threadToPause = t;
				}
			}
		}
	}

	return threadToPause;
}

FThread* Instance::GetThreadToResume() {
	FThread* threadToResume = NULL;

	uint32_t maxPriority = GetHighestPriority();
	uint32_t minimum = numeric_limits<uint32_t>::max();
	for (FThread* t: m_runningThreads) {
		if (t->GetState() == idle || t->GetState() == paused) {
			if (m_enablePriority && t->GetPriority() == maxPriority && t->GetPriority() > 0) {
				threadToResume = t;
				return threadToResume;
			}

			if (t->GetNumTimesResumed() < minimum) { // RoundRobin
				minimum = t->GetNumTimesResumed();
				threadToResume = t;
			}
		}
	}

	return threadToResume;
}

FThread* Instance::GetThreadToMigrate() {
	FThread* threadToMigrate = NULL;

	uint32_t i = 0;
	int pos = -1;
	for (FThread* t: m_runningThreads) {
		uint32_t minimum = numeric_limits<uint32_t>::max();
#ifdef SERVER_VERBOSE
		cout << "t->GetNumTimesResumed(): " << t->GetNumTimesResumed() << endl;
#endif
		if ((t->GetState() == idle || t->GetState() == paused) && t->GetNumTimesResumed() < minimum) {
			minimum = t->GetNumTimesResumed();
			threadToMigrate = t;
			pos = i;
		}
#ifdef SERVER_VERBOSE
		cout << "pos: " << pos << endl;
#endif
		i++;
	}

	if (pos != -1) {
		m_runningThreads.erase(m_runningThreads.begin()+pos);
	}

	return threadToMigrate;
}

void Server::ResumeThread(FThread* fthread, uint32_t whichInstance, bool initiateWithPause) {
	if (fthread == NULL) {
		return;
	}
#ifdef SERVER_VERBOSE
	if (fthread->GetPriority() > 0) {
		cout << "-----------------ResumeThread with id: " << fthread->GetId() << ", priority: " << fthread->GetPriority() << endl;
	}
#endif

#ifdef SERVER_PRINT_STATUS
	cout << "ResumeThread with id: " << fthread->GetId() << endl;
#endif
	fthread->Resume();

	auto output = iFPGA::CastToInt(fthread->m_cML->m_outputHandle);
	output[0] = 0;

	auto programMemory = iFPGA::CastToPtr(fthread->m_cML->m_programMemoryHandle);
	auto inputMemory = iFPGA::CastToPtr(fthread->m_cML->m_inputHandle);
	auto outputMemory = iFPGA::CastToPtr(fthread->m_cML->m_outputHandle);

#ifdef XILINX // On sdaccel we have to trigger context switch already while starting the kernel
	vector<cl::Memory> buffersToCopy;
	buffersToCopy.push_back(outputMemory);
	CopyToFPGA(buffersToCopy);

	uint64_t triggerContextSwitch;
	if (m_enablePriority)
		triggerContextSwitch = m_enableContextSwitch && initiateWithPause && fthread->GetPriority() == 0;
	else
		triggerContextSwitch = m_enableContextSwitch && initiateWithPause;
#ifdef SERVER_PRINT_STATUS
	cout << "---------------------------------------triggerContextSwitch: " << (triggerContextSwitch ? 1 : 0) << endl;
#endif
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

void Server::PauseThread(FThread* fthread, uint32_t whichInstance) {
	if (fthread == NULL) {
		return;
	}

#ifdef SERVER_VERBOSE
	if (fthread->GetPriority() > 0) {
		cout << "-----------------PauseThread with id: " << fthread->GetId() << endl;
	}
#endif

#ifdef SERVER_PRINT_STATUS
	cout << "PauseThread with id: " << fthread->GetId() << endl;
#endif
#ifndef XILINX // We cannot write config registers while a kernel is running in sdaccel
	iFPGA::WriteConfigReg(whichInstance*4 + 0, (vc_select << 30) | (1 << 16) | (fthread->m_cML->m_numInstructions & 0xFF) );
#endif
	fthread->Pause();
}

void Server::ScheduleThreads() {
	for (uint32_t k = 0; k < m_numInstances; k++) {
		FThread* threadToPause = m_instance[k]->GetThreadToPause();
		FThread* threadToResume = m_instance[k]->GetThreadToResume();

		if (m_instance[k]->GetNumRunningThreads() == 0 && threadToResume != NULL) {
			ResumeThread(threadToResume, k, m_instance[k]->GetNumThreads() > 1);
		}
		if (m_enableContextSwitch) {
			PauseThread(threadToPause, k);
		}
	}
}

void Server::RedistributeThreads() {
	vector<Instance*> temp;
	for (uint32_t i = 0; i < m_numInstances; i++) {
		temp.push_back(m_instance[i]);
	}
	stable_sort(temp.begin(), temp.end(), Instance::CompareNumThreads);

	if (temp.front()->GetNumThreads() < temp.back()->GetNumThreads()-1) {
		FThread* fthread = temp.back()->GetThreadToMigrate();
		if (fthread != NULL) {
#ifdef SERVER_VERBOSE
			cout << "Migrate thread with id: " << fthread->GetId() << endl;
			cout << "temp.front()->GetNumThreads(): " << temp.front()->GetNumThreads() << endl;
			cout << "temp.back()->GetNumThreads(): " << temp.back()->GetNumThreads() << endl;
#endif
			temp.front()->AddThread(fthread);
		}
	}
}

void Server::ProcessRequests() {
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
#ifdef SERVER_VERBOSE
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

#ifdef SERVER_PRINT_STATUS
		i++;
		if (i == 50000) {
			cout << "------------------------ (Server " << GetBank() << ") Running threads: " << endl;
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
