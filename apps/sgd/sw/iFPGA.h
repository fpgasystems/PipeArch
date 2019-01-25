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

#define FPGA_MEMORY_SIZE_IN_CL 1024

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

	uint32_t ExampleApp(uint32_t numLines) {

		auto inputHandle = m_fpga->allocBuffer(numLines*64);
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

		uint32_t numIterations = numLines/FPGA_MEMORY_SIZE_IN_CL + (numLines%FPGA_MEMORY_SIZE_IN_CL > 0);
		cout << "numIterations : " << numIterations << endl;

		std::vector<Instruction> instructions;

		Instruction resetInst;
		resetInst.ResetIndex(0);
		resetInst.ResetIndex(1);
		resetInst.ResetIndex(2);

		Instruction prefetchInst;
		prefetchInst.Prefetch(0, numLines, 0, 0, 0);

		access_t accessInternal[2];
		accessInternal[0].m_offsetInCL = 0;
		accessInternal[0].m_lengthInCL = 0;
		accessInternal[1].m_offsetInCL = 0;
		accessInternal[1].m_lengthInCL = numLines;

		Instruction loadInst;
		loadInst.Load(0, numLines, 0, 0, 0, accessInternal, 2);
		loadInst.MakeNonBlocking();

		Instruction writebackInst;
		writebackInst.WriteBack(1, numLines, 0, 0, 0, 1, accessInternal, 2);

		Instruction exitInst;
		exitInst.Jump(2, 0, 0, 0xFFFFFFFF);

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

		m_csrs->writeCSR(0, intptr_t(input));
		m_csrs->writeCSR(1, intptr_t(output));
		m_csrs->writeCSR(2, intptr_t(programMemory));
		m_csrs->writeCSR(3, (uint64_t)instructions.size());

		// Spin, waiting for the value in memory to change to something non-zero.
		struct timespec pause;
		// Longer when simulating
		pause.tv_sec = (m_fpga->hwIsSimulated() ? 1 : 0);
		pause.tv_nsec = 2500000;

		output[0] = 0;
		while (0 == output[0]) {
			nanosleep(&pause, NULL);
		};

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