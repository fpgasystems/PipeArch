#pragma once

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <string>
#include <limits>
#include "Memory.h"

using namespace std;

//-------------------------------------------
// PipeArch
//-------------------------------------------
class PipeArch {
	// std::list<Unit*> m_remoteMemories;
	// std::list<Unit*> m_localMemories;
	// std::list<Unit*> m_localComputations;
protected:
	Unit* m_load;
	Unit* m_writeback;

	void DirectLink(Unit* destination, Unit* source) {
		source->AddOutput(destination);
		destination->AddInput(source);
	}

	void LeftRightLink(Unit* result, Unit* left, Unit* right) {
		left->AddOutput(result);
		right->AddOutput(result);
		result->AddLeftInput(left);
		result->AddRightInput(right);
	}

public:
	PipeArch() {
		m_load = new RemoteVector("load");
		m_writeback = new RemoteVector("writeback");
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
		FileCopy(path + "/pipearch_top.sv", "./output/pipearch_top.sv");
		FileCopy(path + "/sim_sources.txt", "./output/sim_sources.txt");
		FileCopy(path + "/Instruction.h", "../sw");

		FindAndInstert("./output/pipearch_top.sv", "//?LOAD", "parameter NUM_LOAD_CHANNELS = " + to_string(numLoadChannels) + ";");
		FindAndInstert("./output/pipearch_top.sv", "//?WRITEBACK", "parameter NUM_WRITEBACK_CHANNELS = " + to_string(numWriteBackChannels) + ";");

		m_load->Instantiate("./output/pipearch_top.sv");
		m_writeback->Instantiate("./output/pipearch_top.sv");
	}
};