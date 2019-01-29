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


	TempVector input("input", 128);
	LocalVector model("modelMem", 128);
	LocalScalar labels("labelsMem", 128, 0);

	pipeArch.Load(input);
	pipeArch.Load(model);
	pipeArch.Load(labels);

	TempScalar dot("dot", 128);
	pipeArch.Dot(dot, input, model);


	TempScalar error("error", 32);
	pipeArch.ScalarSubtract(error, dot, labels);

	pipeArch.WriteBack(error);

	pipeArch.PrintInfo();
	pipeArch.GenerateVerilog(skeletonPath);

	return 0;
}