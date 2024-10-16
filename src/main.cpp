#include "engine/sol_engine.h"

int main(int argc, char* argv[])
{
	SolEngine engine;

	engine.init();

	engine.run();

	engine.cleanup();

	return 0;
}
