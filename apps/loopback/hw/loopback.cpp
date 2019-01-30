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

	TempVector localFifo("fifo", 512);
	pipeArch.Load(localFifo);

	pipeArch.WriteBack(localFifo);

	pipeArch.PrintInfo();
	pipeArch.GenerateVerilog(skeletonPath);

	return 0;
}