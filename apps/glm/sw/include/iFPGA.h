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

#ifdef XILINX
	#include "xcl2.hpp"

	typedef vector<char,aligned_allocator<char[64]>>* iFPGA_ptr;
#else
	#include "opae_svc_wrapper.h"
	#include "csr_mgr.h"

	// State from the AFU's JSON file, extracted using OPAE's afu_json_mgr script
	#include "afu_json_info.h"

	using namespace std;
	using namespace opae::fpga::types;
	using namespace opae::fpga::bbb::mpf::types;

	typedef shared_buffer::ptr_t iFPGA_ptr;
#endif

class iFPGA {
private:
#ifdef XILINX
	std::vector<cl::Device> m_devices;
	cl::Device m_device;
	cl::Context m_context;
	cl::Program m_program;
	cl::CommandQueue m_queue;
	cl::Kernel m_kernel;
#else
	OPAE_SVC_WRAPPER* m_fpga;
	CSR_MGR* m_csrs;
#endif

public:
	iFPGA() {
#ifdef XILINX
		cl_int err;
		m_devices = xcl::get_xil_devices();
		m_device = m_devices[0];
		m_context = cl::Context(m_device, NULL, NULL, NULL, &err);
		std::string device_name = m_device.getInfo<CL_DEVICE_NAME>(&err);
		// Create Program and Kernel
		std::string binaryFile = xcl::find_binary_file(device_name, "top");
		cl::Program::Binaries bins = xcl::import_binary_file(binaryFile);
		m_devices.resize(1);
		m_program = cl::Program(m_context, m_devices, bins, NULL, &err);
		m_queue = cl::CommandQueue(m_context, m_device, CL_QUEUE_PROFILING_ENABLE, &err);
		m_kernel = cl::Kernel(m_program, "xilinx_top", &err);
#else
		if ( !(strcmp(AFU_ACCEL_UUID,"") == 0) ) {
			m_fpga = new OPAE_SVC_WRAPPER(AFU_ACCEL_UUID);
			assert(m_fpga->isOk());
			m_csrs = new CSR_MGR(*m_fpga);
		}
#endif
	}

	~iFPGA() {
#ifdef XILINX
		// Delete OpenCL objects
#else
		cout << "delete m_csrs" << endl;
		delete m_csrs;
		cout << "delete m_fpga" << endl;
		delete m_fpga;
#endif
	}

	iFPGA_ptr Malloc(size_t size) {
#ifdef XILINX
		iFPGA_ptr result = new vector<char,aligned_allocator<char[64]> >;
		result->reserve(size);
		return result;
#else
		return m_fpga->allocBuffer(size);
#endif
	}

	void Realloc(iFPGA_ptr& handle, size_t size) {
#ifdef XILINX
		if (handle != NULL) {
			handle->clear();
			handle->resize(size);
		}
		else {
			handle = Malloc(size);
		}
#else
		if (handle != NULL) {
			handle->release();
			handle = NULL;
		}
		handle = Malloc(size);
#endif
	}

	void Free(iFPGA_ptr& handle) {
		if (handle != NULL) {
#ifdef XILINX
			handle->clear();
			delete handle;
#else
			handle->release();
#endif
			handle = NULL;
		}
	}

	void WriteConfigReg(uint32_t idx, uint64_t v) {
#ifdef XILINX
		m_kernel.setArg(idx, v);
#else
		m_csrs->writeCSR(idx, v);
#endif
	}

	static volatile uint32_t* CastToInt(iFPGA_ptr& handle) {
		volatile uint32_t* temp = NULL;
#ifdef XILINX
		temp = reinterpret_cast<volatile uint32_t*>(handle->data());
#else
		temp = reinterpret_cast<volatile uint32_t*>(handle->c_type());
#endif
		assert(NULL != temp);
		return temp;
	}

	static volatile float* CastToFloat(iFPGA_ptr& handle) {
		volatile float* temp = NULL;
#ifdef XILINX
		temp = reinterpret_cast<volatile float*>(handle->data());
#else
		temp = reinterpret_cast<volatile float*>(handle->c_type());
#endif
		assert(NULL != temp);
		return temp;
	}

	bool HwIsSimulated() {
#ifdef XILINX
		return false;
#else
		return m_fpga->hwIsSimulated();
#endif
	}

#ifndef XILINX
	void PrintMPF() {
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
#endif
};