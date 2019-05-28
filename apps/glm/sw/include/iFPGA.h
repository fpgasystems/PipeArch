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
#include <unordered_map>

// #define IFPGA_VERBOSE

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


class xDevice {
public:
#ifdef XILINX
	std::vector<cl::Device> m_devices;
	cl::Device m_device;
	cl::Context m_context;
	cl::Program m_program;
	cl::Program::Binaries m_bins;
#endif
	xDevice() {
#ifdef XILINX
		cout << "xDevice constructor" << endl;
		cl_int err;
		m_devices = xcl::get_xil_devices();
		m_device = m_devices[0];
		m_context = cl::Context(m_device, NULL, NULL, NULL, &err);
		std::string device_name = m_device.getInfo<CL_DEVICE_NAME>(&err);
		// Create Program and Kernel
		std::string binaryFile = xcl::find_binary_file(device_name, "mytop");
		m_bins = xcl::import_binary_file(binaryFile);
		m_devices.resize(1);
		m_program = cl::Program(m_context, m_devices, m_bins, NULL, &err);
#endif
	}
};

class iFPGA {
public:
	static const uint32_t PREFETCH_BUFFER_SIZE_IN_CL = (1 << 10);
	static const uint32_t MEMORY_SIZE_IN_CL = (1 << 10);
	static const uint32_t MAX_NUM_INSTANCES = MAKEFILE_MAX_NUM_INSTANCES;
	static const uint32_t MAX_NUM_BANKS = MAKEFILE_MAX_NUM_BANKS;
	const uint32_t MAX_NUM_CLS_DRAM = 268435456; // 16 GB

protected:
	uint32_t m_numInstances;

private:
	uint32_t m_whichBank;
	uint32_t m_numCLsAllocated;
	unordered_map<iFPGA_ptr, bool> m_copiedBuffers;

#ifdef XILINX
	xDevice* m_xdevice = nullptr;
	cl::CommandQueue m_queue[MAX_NUM_INSTANCES];
	cl::Kernel m_kernel;
#else
	OPAE_SVC_WRAPPER* m_fpga;
	CSR_MGR* m_csrs;
#endif

public:

	iFPGA(uint32_t numInstances, uint32_t whichBank, xDevice* xdevice) {
		m_numInstances = numInstances;
		m_whichBank = whichBank;
		m_numCLsAllocated = 0;
		m_copiedBuffers = unordered_map<iFPGA_ptr, bool>();
#ifdef XILINX
		m_xdevice = xdevice;
		cout << "m_whichBank: " << m_whichBank << endl;
		cl_int err;
		if (m_whichBank == 0) {
			m_kernel = cl::Kernel(m_xdevice->m_program, "xilinx_top", &err);
		}
		else if (m_whichBank == 1) {
			m_kernel = cl::Kernel(m_xdevice->m_program, "xilinx_top_i", &err);
		}
		else if (m_whichBank == 2) {
			m_kernel = cl::Kernel(m_xdevice->m_program, "xilinx_top_ii", &err);
		}
		else if (m_whichBank == 3) {
			m_kernel = cl::Kernel(m_xdevice->m_program, "xilinx_top_iii", &err);
		}

		for (uint32_t i = 0; i < m_numInstances; i++) {
			// m_queue[i] = cl::CommandQueue(m_xdevice->m_context, m_xdevice->m_device, CL_QUEUE_PROFILING_ENABLE, &err);
			// m_queue[i] = cl::CommandQueue(m_xdevice->m_context, m_xdevice->m_device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);
			m_queue[i] = cl::CommandQueue(m_xdevice->m_context, m_xdevice->m_device);
		}
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
		char* temp = (char*)aligned_alloc(4096, size);
		cl_int err;
		cl_mem_ext_ptr_t ext_ptr;
		if (m_whichBank == 0) {
			ext_ptr.flags = XCL_MEM_DDR_BANK0;
		}
		else if (m_whichBank == 1) {
			ext_ptr.flags = XCL_MEM_DDR_BANK1;
		}
		else if (m_whichBank == 2) {
			ext_ptr.flags = XCL_MEM_DDR_BANK2;
		}
		else if (m_whichBank == 3) {
			ext_ptr.flags = XCL_MEM_DDR_BANK3;
		}
		ext_ptr.obj = (void*)temp;
		ext_ptr.param = 0;
		cl::Buffer* buffer = new cl::Buffer(m_xdevice->m_context, CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR, size, &ext_ptr, &err);
		return buffer;
#else
		return m_fpga->allocBuffer(size);
#endif
	}

	uint32_t GetBank() {
		return m_whichBank;
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
		string name;
		m_kernel.getInfo(CL_KERNEL_FUNCTION_NAME, &name);
		m_kernel.setArg(idx, v);
#else
		m_csrs->writeCSR(idx, v);
#endif
	}

#ifdef XILINX
	void WriteConfigReg(uint32_t idx, cl::Buffer& v) {
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
	bool DoesInputHandleFitDRAM(iFPGA_ptr handle, uint32_t totalSizeInCL) {
#ifdef IFPGA_VERBOSE
		cout << "DoesThreadFitDRAM" << endl;
#endif
		if( m_copiedBuffers.find(handle) != m_copiedBuffers.end() ) {
// #ifdef IFPGA_VERBOSE
			cout << "----Handle already copied" << endl;
// #endif
			return true;
		}
		else {
// #ifdef IFPGA_VERBOSE
			cout << "----Need to copy handle. Size: " << totalSizeInCL << endl;
			cout << "m_numCLsAllocated: " << m_numCLsAllocated << endl;
// #endif
			return (totalSizeInCL < MAX_NUM_CLS_DRAM - m_numCLsAllocated);
		}
	}

	bool HandleNeedsCopying(iFPGA_ptr handle) {
		return (m_copiedBuffers.find(handle) == m_copiedBuffers.end());
	}

	void CopyInputHandleToFPGA(iFPGA_ptr handle, uint32_t totalSizeInCL) {
		cout << "Copying " << totalSizeInCL*64/1e9 << " GBs to bank " << GetBank() << endl;
		vector<cl::Memory> buffersToCopy;
		buffersToCopy.push_back(iFPGA::CastToPtr(handle));
		CopyToFPGA(buffersToCopy);
		m_copiedBuffers.insert({handle, true});
		m_numCLsAllocated += totalSizeInCL;
		cout << "Allocated: " << m_numCLsAllocated*64/1e9 << " GBs" << endl;
	}

	void RemoveCopiedHandle(iFPGA_ptr handle, uint32_t totalSizeInCL) {
		m_copiedBuffers.erase(handle);
		m_numCLsAllocated -= totalSizeInCL;
	}
#endif

#ifdef XILINX
	void CopyToFPGA(vector<cl::Memory>& buffersToCopy) {
		cl_int err;
#ifdef IFPGA_VERBOSE
		cout << "CopyToFPGA" << endl;
		double start = get_time();
#endif
		err = m_queue[0].enqueueMigrateMemObjects(buffersToCopy, 0/* 0 means from host*/);
		err = m_queue[0].finish();
#ifdef IFPGA_VERBOSE
		double end = get_time();
		cout << "Copy time: " << end - start << endl;
#endif
	}

	void CopyFromFPGA(vector<cl::Memory>& buffersToCopy, uint32_t whichInstance) {
		cl_int err;
#ifdef IFPGA_VERBOSE
		err = m_queue[whichInstance].finish();
		cout << "CopyFromFPGA" << endl;
#endif
		err = m_queue[whichInstance].enqueueMigrateMemObjects(buffersToCopy, CL_MIGRATE_MEM_OBJECT_HOST);
	}

	void CopyFromFPGASingle(iFPGA_ptr& bufferToCopy, uint32_t whichInstance) {
		cl_int err;
#ifdef IFPGA_VERBOSE
		err = m_queue[whichInstance].finish();
		cout << "CopyFromFPGA single buffer" << endl;
#endif
		vector<cl::Memory> temp;
		temp.push_back(CastToPtr(bufferToCopy));
		err = m_queue[whichInstance].enqueueMigrateMemObjects(temp, CL_MIGRATE_MEM_OBJECT_HOST);
		err = m_queue[whichInstance].finish();
	}

	void StartKernel(uint32_t whichInstance) {
		cl_int err;
#ifdef IFPGA_VERBOSE
		cout << "StartKernel " << whichInstance << endl;
		double start = get_time();
#endif
		err = m_queue[whichInstance].enqueueTask(m_kernel);
#ifdef IFPGA_VERBOSE
		err = m_queue[whichInstance].finish();
		double end = get_time();
		cout << "Kernel time: " << end - start << endl;
#endif
	}

	uint32_t GetQueueCount(uint32_t whichInstance) {
		uint32_t count = 1;
		m_queue[whichInstance].getInfo(CL_QUEUE_REFERENCE_COUNT, &count);
		cout << "count: " << count << endl;
		return count;
	}

	void WaitForKernel(uint32_t whichInstance) {
#ifdef IFPGA_VERBOSE
		cout << "WaitForKernel" << endl;
#endif
		cl_int err = m_queue[whichInstance].finish();
	}
#endif
};