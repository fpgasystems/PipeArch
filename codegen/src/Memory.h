#pragma once

#include "Unit.h"
#include <math.h>

class LocalMemory : public Unit {
private:
	unsigned m_size;
	unsigned m_log2size;

public:
	LocalMemory(string name, OutputWidth outputWidth, unsigned size) : Unit(name, outputWidth, t_local) {
		m_size = size;
		float temp = log2(size);
		m_log2size = (unsigned)(temp+0.5);
	}

	void Instantiate(string& fileName) {
		if (!m_instantiated) {
			cout << "Instantiate " << m_name << endl;
			string width;
			if (m_outputWidth == t_vector) {
				width = "512";
			}
			else {
				width = "32";
			}
			string instantiation("");
			instantiation += "fifobram_interface #(.WIDTH(" + width + "), .LOG2_DEPTH(" + to_string(m_log2size) + ")) " + m_interfaceName + "();\n";
			instantiation += "bram\n";
			instantiation += "#(.WIDTH(" + width + "), .LOG2_DEPTH(" + to_string(m_log2size) + "))\n";
			instantiation += m_name + "_inst (\n";
			instantiation += ".clk,\n";
			instantiation += ".access(" + m_interfaceName + ".bram_source)\n";
			instantiation += ");\n";
			FindAndInstert(fileName, "//?LOCALMEM", instantiation);
			for (Unit* output: m_outputs) {
				if (output->GetName().compare("writeback") != 0) {
					output->Instantiate(fileName);
				}
			}
			m_instantiated = true;
		}
		else {
			cout << m_name << " already instantiated" << endl;
		}
	}
};

class LocalVector : public LocalMemory{
public:
	LocalVector(string name, unsigned size) : LocalMemory(name, t_vector, t_local) {}

	void Instantiate(string& fileName) {
		LocalMemory::Instantiate(fileName);
	}
};

class LocalScalar : public LocalMemory{
public:
	LocalScalar(string name, unsigned size) : LocalMemory(name, t_scalar, t_local) {}

	void Instantiate(string& fileName) {
		LocalMemory::Instantiate(fileName);
	}
};

class TempMemory : public Unit {
private:
	unsigned m_size;
	unsigned m_log2size;

public:
	TempMemory(string name, OutputWidth outputWidth, unsigned size) : Unit(name, outputWidth, t_temporary) {
		m_size = size;
		float temp = log2(size);
		m_log2size = (unsigned)(temp+0.5);
	}

	void Instantiate(string& fileName) {
		if (!m_instantiated) {
			cout << "Instantiate " << m_name << endl;
			string width;
			if (m_outputWidth == t_vector) {
				width = "512";
			}
			else {
				width = "32";
			}
			string instantiation("");
			instantiation += "fifobram_interface #(.WIDTH(" + width + "), .LOG2_DEPTH(" + to_string(m_log2size) + ")) " + m_interfaceName + "();\n";
			instantiation += "fifo\n";
			instantiation += "#(.WIDTH(" + width + "), .LOG2_DEPTH(" + to_string(m_log2size) + "))\n";
			instantiation += m_name + "_inst (\n";
			instantiation += ".clk, .reset,\n";
			instantiation += ".access(" + m_interfaceName + ".fifo_source)\n";
			instantiation += ");\n";
			FindAndInstert(fileName, "//?LOCALMEM", instantiation);

			for (Unit* output: m_outputs) {
				if (output->GetName().compare("writeback") != 0) {
					output->Instantiate(fileName);
				}
			}

			m_instantiated = true;
		}
		else {
			cout << m_name << " already instantiated" << endl;
		}
	}
};

class TempVector : public TempMemory{
public:
	TempVector(string name, unsigned size) : TempMemory(name, t_vector, t_temporary) {}

	void Instantiate(string& fileName) {
		TempMemory::Instantiate(fileName);
	}
};

class TempScalar : public TempMemory{
public:
	TempScalar(string name, unsigned size) : TempMemory(name, t_scalar, t_temporary) {}

	void Instantiate(string& fileName) {
		TempMemory::Instantiate(fileName);
	}
};