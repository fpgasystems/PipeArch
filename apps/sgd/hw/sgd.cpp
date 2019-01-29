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


	TempVector input1("input1", 128);
	TempVector input2("input2", 128);

	TempScalar dot("dot", 128);
	pipeArch.Dot(dot, input1, input2);


	pipeArch.WriteBack(dot);
	pipeArch.Load(input1);
	pipeArch.Load(input2);

	pipeArch.PrintInfo();
	pipeArch.GenerateVerilog(skeletonPath);

	return 0;
}