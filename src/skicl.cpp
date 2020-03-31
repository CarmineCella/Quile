// skicl.cpp
//

#include "skicl.h"
#include <iostream>
#include <string>
#include <stdexcept>

using namespace std;

int main (int argc, char* argv[]) {
	try {
	    AtomPtr env = make_env ();
		if (argc > 1) {
			for (unsigned i = 1; i < argc; ++i) {
				source (argv[i], env);
			}
		} else {
			cout << "[skicl, ver 0.1]" << endl << endl;
			cout << "a tiny tcl dialect" << endl;
			cout << "(c) 2020, www.carminecella.com" << endl << endl;
            repl (cin, env);
		}
	} catch (exception& e) {
		cout << "error: " << e.what () << endl;
	} catch (...) {
		cout << "fatal error; quitting" << endl;
	}

    return 0;
}