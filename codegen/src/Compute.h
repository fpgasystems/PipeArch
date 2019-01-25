#pragma once

#include <stdlib.h>
#include <iostream>
#include <list>
#include <string>

using namespace std;

class Compute {
private:
	string m_name;

public:
	Compute(string name) {
		m_name = name;
	}

	string GetName() {
		return m_name;
	}
};