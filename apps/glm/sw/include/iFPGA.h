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
	typedef cl::Buffer* iFPGA_ptr;
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
		std::string binaryFile = xcl::find_binary_file(device_name, "mytop");
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
		char* temp = (char*)aligned_alloc(64, size);
		cl_int err;
		cl_mem_ext_ptr_t ext_ptr;
		ext_ptr.flags = XCL_MEM_DDR_BANK0;
		ext_ptr.obj = (void*)temp;
		ext_ptr.param = 0;
		cl::Buffer* buffer = new cl::Buffer(m_context, CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR, size, &ext_ptr, &err);
		return buffer;
#else
		return m_fpga->allocBuffer(size);
#endif
	}

	void Realloc(iFPGA_ptr& handle, size_t size) {
#ifdef XILINX
		if (handle != NULL) {
			delete handle;
			handle = NULL;
		}
		handle = Malloc(size);
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
			delete handle;
#else
			handle->release();
#endif
			handle = NULL;
		}
	}

	void WriteConfigReg(uint32_t idx, uint64_t v) {
#ifdef XILINX
		cout << "Writing argument" << endl;
		string name;
		m_kernel.getInfo(CL_KERNEL_FUNCTION_NAME, &name);

		cout << "m_kernel name: " << name << endl;
		m_kernel.setArg(idx, v);
#else
		m_csrs->writeCSR(idx, v);
#endif
	}

#ifdef XILINX
	void WriteConfigReg(uint32_t idx, cl::Buffer& v) {
		cout << "Writing cl::Buffer" << endl;
		m_kernel.setArg(idx, v);
	}
#else
	void WriteConfigReg(uint32_t idx, volatile void* v) {
		m_csrs->writeCSR(idx, intptr_t(v));
	}
#endif

#ifdef XILINX
	static cl::Buffer CastToPtr(iFPGA_ptr handle) {
		return *handle;
	}
#else
	static volatile void* CastToPtr(iFPGA_ptr handle) {
		volatile void* temp = reinterpret_cast<volatile void*>(handle->c_type());
		assert(NULL != temp);
		return temp;
	}
#endif

	static volatile uint32_t* CastToInt(iFPGA_ptr& handle) {
		volatile uint32_t* temp = NULL;
#ifdef XILINX
		void* temp2 = 0;
		handle->getInfo(CL_MEM_HOST_PTR, &temp2);
		cout << "temp2: " << temp2 << endl;
		temp = reinterpret_cast<volatile uint32_t*>(temp2);
#else
		temp = reinterpret_cast<volatile uint32_t*>(handle->c_type());
#endif
		assert(NULL != temp);
		return temp;
	}

	static volatile float* CastToFloat(iFPGA_ptr& handle) {
		volatile float* temp = NULL;
#ifdef XILINX
		void* temp2 = NULL;
		handle->getInfo(CL_MEM_HOST_PTR, &temp2);
		temp = reinterpret_cast<volatile float*>(temp2);
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

#ifdef XILINX
	void CopyToFPGA(vector<cl::Memory>& buffersToCopy) {
		cout << "CopyToFPGA" << endl;
		cl_int err = m_queue.enqueueMigrateMemObjects(buffersToCopy, 0/* 0 means from host*/);
		err = m_queue.finish();
	}

	void CopyFromFPGA(vector<cl::Memory>& buffersToCopy) {
		cout << "CopyFromFPGA" << endl;
		cl_int err = m_queue.enqueueMigrateMemObjects(buffersToCopy, CL_MIGRATE_MEM_OBJECT_HOST);
		err = m_queue.finish();
	}

	void StartKernel() {
		cout << "StartKernel" << endl;
		cl_int err;
		err = m_queue.enqueueTask(m_kernel);
	}

	void WaitForKernel() {
		cout << "WaitForKernel" << endl;
		cl_int err = m_queue.finish();
	}
#endif
};