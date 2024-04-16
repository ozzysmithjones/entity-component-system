// entity_component_system.cpp : Defines the entry point for the application.
//

#include "entity_component_system.h"
#include <iostream>
using namespace std;

using Human = ecs::EntityArchetype<int, bool, float>;
using Goblin = ecs::EntityArchetype<int, float>;



int main()
{
	ecs::Scene<Human, Goblin> scene;

	for (int i = 0; i < 100; ++i) {
		scene.create_entity<Human>();
	}

	for (int i = 0; i < 100; ++i) {
		scene.create_entity<Goblin>();
	}

	ecs::EntityHandle to_destroy;
	to_destroy.id = 1;
	to_destroy.archetype_index = 0;
	scene.destroy_entity(to_destroy);
	scene.for_each([&scene](ecs::EntityHandle handle, int num, float f) 
		{
			std::cout << "entity id = " << handle.id << " integer = " << num << " float = " << num;
			auto [a, b] = scene.get_components<int, float>(handle);
			std::cout << " a = " << *a << " b = " << *b << "\n";
		});

	//std::cout << a << "\n" << b << "\n" << c << "\nhello there!\n";
	//arc.pop_back();
	return 0;
}
