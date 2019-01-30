#include "PipeArchBase.h"
#include "Memory.h"
#include <iostream>

int main(int argc, char* argv[]) {

	char* skeletonPath;
	if (argc != 2) {
		cout << "Usage: app </path/to/skeleton>" << endl;
		exit(1);
	}
	skeletonPath = argv[1];

	PipeArchBase pipeArch;

	TempVector input("input", 512);
	TempVector samplesForward("samplesForward", 4096);
	LocalVector model("modelMem", 4096);
	LocalVector labels("labelsMem", 2048);

	pipeArch.Load(input);
	pipeArch.Load(samplesForward);
	pipeArch.Load(model);
	pipeArch.Load(labels);

	pipeArch.WriteBack(model);
	pipeArch.WriteBack(labels);

	pipeArch.PrintInfo();
	pipeArch.GenerateVerilog(skeletonPath);

	return 0;
}