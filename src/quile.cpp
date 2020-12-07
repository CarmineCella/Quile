// quile.cpp
//

#include "quile.h"

#include <iostream>
#include <sstream>
#include <signal.h>
#include <stdexcept>
#include <unistd.h>

using namespace std;

const char* PACKAGE[] = {"quile"};
const char* VERSION[] = {"0.2"};

int main (int argc, char* argv[]) {
	AtomPtr env = make_env (); // master environment

	try {
		bool interactive = false;
		int opt = 0;
		while ((opt = getopt(argc, argv, "i")) != -1) {
		    switch (opt) {
		    case 'i': interactive = true; break;
		    default:
		        std::stringstream msg;
		        msg << "usage is " << argv[0] << " [-i] [file...]";
		        throw runtime_error (msg.str ());
		    }
		}

		if (argc - optind == 0) {
			cout << BOLDBLUE << "[" << *PACKAGE << ", version "
				<< *VERSION <<"]" << RESET << endl << endl;

			cout << "scripting language" << endl;
			cout << "(c) 2020, www.quile.org" << endl << endl;

			repl (env, cin, cout);
		} else {
			for (int i = optind; i < argc; ++i) {
				source (argv[i], env);
			}
			if (interactive) repl (env, cin, cout);
		}
	} catch (std::exception& e) {
			cout << RED << "error: " << e.what () << RESET << std::endl;
	} catch (AtomPtr& e) {
		cout << RED << "error: uncaught expection " << RESET;
		puts (e, cout); cout << std::endl;
	} catch (...) {
		cout << RED << "fatal error: execution stopped" << RESET << std::endl;
	}
	return 0;
}

// eof
