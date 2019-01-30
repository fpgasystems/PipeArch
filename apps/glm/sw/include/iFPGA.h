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

};