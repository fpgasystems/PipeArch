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

#include "opae_svc_wrapper.h"
#include "csr_mgr.h"

using namespace std;
using namespace opae::fpga::types;
using namespace opae::fpga::bbb::mpf::types;

class iFPGA {
private:
	OPAE_SVC_WRAPPER* m_fpga;
	CSR_MGR* m_csrs;

public:
	iFPGA(const char* accel_uuid) {
		if ( !(strcmp(accel_uuid,"") == 0) ) {
			m_fpga = new OPAE_SVC_WRAPPER(accel_uuid);
			assert(m_fpga->isOk());
			m_csrs = new CSR_MGR(*m_fpga);
		}
	}

	~iFPGA() {
		cout << "delete m_csrs" << endl;
		delete m_csrs;
		cout << "delete m_fpga" << endl;
		delete m_fpga;
	}

	shared_buffer::ptr_t malloc(size_t size) {
		return m_fpga->allocBuffer(size);
	}

	void writeCSR(uint32_t idx, uint64_t v) {
		m_csrs->writeCSR(idx, v);
	}

	bool hwIsSimulated() {
		return m_fpga->hwIsSimulated();
	}

	void printMPF() {
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