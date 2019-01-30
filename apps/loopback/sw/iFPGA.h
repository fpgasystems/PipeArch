#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <iostream>
#include <string>
#include <atomic>
#include <vector>
#include <memory>

#include "Instruction.h"

#include "opae_svc_wrapper.h"
#include "csr_mgr.h"

using namespace std;
using namespace opae::fpga::types;
using namespace opae::fpga::bbb::mpf::types;

static double get_time() {
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec + t.tv_usec*1e-6;
}

struct MemoryChunk {
	uint32_t m_offsetInCL;
	uint32_t m_lenghtInCL;
};

class iFPGA {
public:
	OPAE_SVC_WRAPPER* m_fpga;
	CSR_MGR* m_csrs;
	shared_buffer::ptr_t m_handle;

	iFPGA(const char* accel_uuid) {
		m_fpga = new OPAE_SVC_WRAPPER(accel_uuid);
		assert(m_fpga->isOk());
		m_csrs = new CSR_MGR(*m_fpga);
		m_handle = NULL;
	}

	~iFPGA() {
		cout << "m_handle->release()" << endl;
		if (m_handle != NULL) {
			m_handle->release();
			m_handle = NULL;
		}
		cout << "delete m_csrs" << endl;
		delete m_csrs;
		cout << "delete m_fpga" << endl;
		delete m_fpga;
	}

	uint32_t ExampleApp(uint32_t numLines, uint32_t numIterations) {

		if (numLines > 32768) {
			cout << "max allowed numLines is 32768" << endl;
			exit(1);
		}

		auto inputHandle = m_fpga->allocBuffer(numLines*numIterations*64);
		auto input = reinterpret_cast<volatile float*>(inputHandle->c_type());
		assert(NULL != input);

		for (uint32_t i = 0; i < numLines*16; i++) {
			input[i] = i;
		}

		auto outputHandle = m_fpga->allocBuffer((numLines+1)*64);
		auto output = reinterpret_cast<volatile float*>(outputHandle->c_type());
		assert(NULL != output);

		for (uint32_t i = 0; i < numLines*16; i++) {
			output[i] = 0;
		}

		std::vector<Instruction> instructions;

		Instruction prefetchInst;
		prefetchInst.Prefetch(0, numLines*numIterations, 0, 0, 0);
		prefetchInst.ResetIndex(0);
		prefetchInst.ResetIndex(1);
		prefetchInst.ResetIndex(2);

		access_t accessRead[1];
		accessRead[0].m_offsetInCL = 0;
		accessRead[0].m_lengthInCL = numLines;
		Instruction loadInst;
		loadInst.Load(0, numLines, 0, 0, 0, accessRead, 1);
		loadInst.MakeNonBlocking();

		access_t accessWrite[1];
		accessWrite[0].m_offsetInCL = 0;
		accessWrite[0].m_lengthInCL = numLines;
		Instruction writebackInst;
		writebackInst.WriteBack(1, numLines, 0, 0, 0, 0, accessWrite, 1);
		writebackInst.IncrementIndex(2);

		Instruction exitInst;
		exitInst.Jump(2, numIterations, 1, 0xFFFFFFFF);

		instructions.push_back(prefetchInst);
		instructions.push_back(loadInst);
		instructions.push_back(writebackInst);
		instructions.push_back(exitInst);

		// Copy program to FPGA memory
		auto programMemoryHandle = m_fpga->allocBuffer(instructions.size()*64);
		auto programMemory = reinterpret_cast<volatile uint32_t*>(programMemoryHandle->c_type());
		uint32_t k = 0;
		for (Instruction i: instructions) {
			i.Copy(programMemory + k*Instruction::NUM_WORDS);
			k++;
		}

		// Spin, waiting for the value in memory to change to something non-zero.
		struct timespec pause;
		// Longer when simulating
		pause.tv_sec = (m_fpga->hwIsSimulated() ? 1 : 0);
		pause.tv_nsec = 100;

		double start = get_time();

		m_csrs->writeCSR(0, intptr_t(input));
		m_csrs->writeCSR(1, intptr_t(output));
		m_csrs->writeCSR(2, intptr_t(programMemory));
		m_csrs->writeCSR(3, (uint64_t)instructions.size());

		output[0] = 0;
		while (0 == output[0]) {
			nanosleep(&pause, NULL);
		};

		double end = get_time();
		cout << "Time: " << end-start << endl;

		// Check values
		bool pass = true;
		for (uint32_t i = 0; i < numLines*16; i++) {
			if (input[i] != output[i+16]) {
				cout << "Mismatch at index " << i << ". input: " << input[i] << ", output: " << output[i+16] << endl;
				pass = false;
			}
		}
		if (pass) {
			cout << "PASS!" << endl;
		}

		// Reads CSRs to get some statistics
		cout	<< "# List length: " << m_csrs->readCSR(0) << endl
				<< "# Linked list data entries read: " << m_csrs->readCSR(1) << endl;

		cout	<< "#" << endl
				<< "# AFU frequency: " << m_csrs->getAFUMHz() << " MHz"
				<< (m_fpga->hwIsSimulated() ? " [simulated]" : "")
				<< endl;

		// MPF VTP (virtual to physical) statistics
		mpf_handle::ptr_t mpf = m_fpga->mpf;
		if (mpfVtpIsAvailable(*mpf))
		{
			mpf_vtp_stats vtp_stats;
			mpfVtpGetStats(*mpf, &vtp_stats);

			cout << "#" << endl;
			if (vtp_stats.numFailedTranslations)
			{
				cout << "# VTP failed translating VA: 0x" << hex << uint64_t(vtp_stats.ptWalkLastVAddr) << dec << endl;
			}
			cout	<< "# VTP PT walk cycles: " << vtp_stats.numPTWalkBusyCycles << endl
					<< "# VTP L2 4KB hit / miss: " << vtp_stats.numTLBHits4KB << " / "
					<< vtp_stats.numTLBMisses4KB << endl
					<< "# VTP L2 2MB hit / miss: " << vtp_stats.numTLBHits2MB << " / "
					<< vtp_stats.numTLBMisses2MB << endl;
		}
	}
};