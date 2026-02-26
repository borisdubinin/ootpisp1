#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include <iostream>

int main()
{
    // Optionally remove VideoMode::getDesktopMode() logic for a regular window during dev
    sf::RenderWindow window(sf::VideoMode(1280, 720), "GraphEditor");
    window.setFramerateLimit(60);

    if (!ImGui::SFML::Init(window))
    {
        std::cerr << "Failed to initialize ImGui-SFML" << std::endl;
        return -1;
    }

    sf::Clock deltaClock;
    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(window, event);

            if (event.type == sf::Event::Closed)
            {
                window.close();
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // Basic ImGui Panel
        ImGui::Begin("Properties");
        ImGui::Text("Hello, GraphEditor!");
        ImGui::End();

        window.clear(sf::Color(240, 240, 240)); // light gray background

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}
