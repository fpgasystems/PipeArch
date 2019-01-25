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

	RemoteVector inputData("input");

	LocalVector localMemory("local", 1000);
	pipeArch.Copy(localMemory, inputData);

	TempVector localFifo("fifo", 128);
	pipeArch.Copy(localFifo, inputData);

	pipeArch.WriteBack(localMemory);
	pipeArch.WriteBack(localFifo);

	pipeArch.Load(inputData);

	pipeArch.PrintInfo();
	pipeArch.GenerateVerilog(skeletonPath);

	return 0;
}