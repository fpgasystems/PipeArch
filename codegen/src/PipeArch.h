#pragma once

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <string>
#include <limits>
#include "Compute.h"
#include "Memory.h"

using namespace std;

//-------------------------------------------
// PipeArch
//-------------------------------------------
class PipeArch {
protected:
	Unit* m_load;
	Unit* m_writeback;
	unsigned m_numOps;

	void DirectLink(Unit* destination, Unit* source) {
		source->AddOutput(destination);
		destination->AddInput(source);
	}

	void LeftRightLink(Compute* result, Unit* left, Unit* right) {
		left->AddOutput(result);
		right->AddOutput(result);
		result->AddLeftInput(left);
		result->AddRightInput(right);
	}

public:
	PipeArch() {
		m_load = new Load("load");
		m_writeback = new WriteBack("writeback");
		m_numOps = 2;
	}

	void PrintInfo() {
		m_load->PrintInfo("");
		m_writeback->PrintInfo("");
	}

	void GenerateVerilog(char* skeletonPath) {
		cout << "************** GenerateVerilog **************" << endl;

		unsigned numLoadChannels = m_load->GetOutputs().size();
		cout << "numLoadChannels: " << numLoadChannels << endl;

		unsigned numWriteBackChannels = m_writeback->GetInputs().size();
		cout << "numWriteBackChannels: " << numWriteBackChannels << endl;

		struct stat info;
		if (stat("output", &info) == 0) {
			system("rm -r output");
		}
		system("mkdir output");

		string path(skeletonPath);
		string pipearch_top("./output/pipearch_top.sv");
		FileCopy(path + "/pipearch_top.sv", pipearch_top);
		FileCopy(path + "/sim_sources.txt", "./output/sim_sources.txt");
		FileCopy(path + "/Instruction.h", "../sw");

		FindAndInstert(pipearch_top, "//?LOAD", "parameter NUM_LOAD_CHANNELS = " + to_string(numLoadChannels) + ";");
		FindAndInstert(pipearch_top, "//?WRITEBACK", "parameter NUM_WRITEBACK_CHANNELS = " + to_string(numWriteBackChannels) + ";");

		m_load->Instantiate(pipearch_top);
		m_writeback->Instantiate(pipearch_top);
	}
};