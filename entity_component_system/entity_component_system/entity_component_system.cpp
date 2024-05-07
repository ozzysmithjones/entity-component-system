// entity_component_system.cpp : Defines the entry point for the application.
//

#include "entity_component_system.h"

#include <iostream>
using Human = ecs::EntityArchetype<int, bool, float>;
using Goblin = ecs::EntityArchetype<int, float>;
using namespace std;


int main()
{
	ecs::Scene<Human, Goblin> scene;

	for (int i = 0; i < 100; ++i) {
		scene.create_entity<Human>();
	}

	ecs::EntityHandle to_destroy;
	to_destroy.id = 1;
	to_destroy.archetype_index = 0;
	scene.destroy_entity(to_destroy);

	for (int i = 0; i < 100; ++i) {
		scene.create_entity<Goblin>();
	}

	ecs::EntityHandle to_find;
	to_find.id = (1ull << 32) + 1;
	to_find.archetype_index = 1;

	{
		auto [health, speed] = scene.get_components<int, float>(to_find);
		*health = 100;
		*speed = 3.3f;
	}
	
	scene.for_each([](ecs::EntityHandle handle, int num, float f) 
		{
			std::cout << "entity id = " << handle.id << " integer = " << num << " float = " << f << '\n';
		});

	std::optional<ecs::EntityHandle> e = scene.find_entity_where([](float f) { return f > 0.0f; });
	if (!e.has_value()) {
		std::cerr << "Failed to find entity!\n";
		return 0;
	}

	{
		auto [health, speed] = scene.get_components<int, float>(e.value());
		*health = 200;
		*speed = 6.6f;
	}


	scene.destroy_entities_where([](int health) { return health <= 0; });
	std::cout << "\n\nAlive entities:\n\n";
	scene.for_each([](ecs::EntityHandle handle, int num, float f)
		{
			std::cout << "entity id = " << handle.id << " integer = " << num << " float = " << f << '\n';
		});
	
	//std::cout << a << "\n" << b << "\n" << c << "\nhello there!\n";
	//arc.pop_back();
	return 0;
}
