//
// timer.cpp
// ~~~~~~~~~
//
// Copyright (c) 2003-2016 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <thread>
#include <queue>
#include <typeinfo>
#include <chrono>
#include <ranges>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui_stdlib.h"

#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include "clientmain.h"
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

bool toggle = true;

template<typename ... lambdas>
struct overload : lambdas... {
    using lambdas::operator()...;
};

namespace flags {
    constexpr auto main = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoTitleBar;
}


struct GuiState {

    enum class View {
        menu,
        chat
    };

    GuiState(const std::vector<std::string> in) {
        for (const auto item : in) {
            inputs.insert({ item, "" });
        }
        client_ = std::make_unique<Client>();
    }

    std::string readMessage() {
        std::lock_guard lock{ messageMutex };
        auto mes = std::move(message);
        message = "";
        return mes;
    }

    void writeMessage(const std::string& newMessage) {
        std::cout << "writing message" << std::endl;
        std::lock_guard lock{ messageMutex };
        message.replace(std::begin(message), std::end(message), newMessage);
    }

    void makeConnection(std::string host_name, uint16_t port) {
        if (!(inputs.find("username")->second.empty())) {
            
            client_->ConnectToServer("127.0.0.1", 60000);
            connected = true;
            currentView = View::chat;
        }
        else {
            std::cout << "please input a username to connect to the server" << std::endl;
        }
    }

    std::map<std::string, std::string> inputs;
    View currentView { View::menu };
    std::string message;
    std::mutex messageMutex;
    shriller::netv2::safequeue messageQueue;
    bool generateNames{ false };
    bool init{ false };
    bool connected{false};
    std::unique_ptr<Client> client_ = nullptr;
    
};

struct SyncedState {
    std::string localUsername{"anoymous"};
    std::vector<std::tuple<std::string, std::string>> messageHistory;
    std::vector<std::string> onlineUsers_;
};

struct clientThreads {
        
    std::jthread main;
    std::jthread write;
    std::jthread read;

};

class InputTextWrapper {

public:
    InputTextWrapper(size_t width, const char* label, bool visible) :
        width_{ width }, 
        label_{ label }, 
        visible_{visible}
    {}

    void Render(std::string& message) {
        if (visible_) {
            ImGui::PushItemWidth(width_);
            ImGui::InputText(label_, &message);
            ImGui::PopItemWidth();
        }
    }

private:
    size_t width_;
    const char* label_;
    bool visible_;

};

void topPane(const std::unique_ptr<std::pair<GuiState, SyncedState>>& gState_p) {

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;

 
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImColor(61,60,59,255).Value);
    ImGui::PushStyleColor(ImGuiCol_Text, ImColor(255, 255, 255).Value);

    ImGui::BeginChild(
        "ChildL",
        ImVec2(ImGui::GetContentRegionAvail().x * 0.60f, ImGui::GetContentRegionAvail().y * 0.80f),
        false, 
        window_flags
    );

    for (const auto& [username, message] : gState_p->second.messageHistory) {
        ImGui::Text(("[ " + username + " ] : " + message).c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    
    ImGui::SameLine();

    ImGui::BeginChild("ChildTwo", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.80f), false, window_flags);
    
    ImGui::Text("Users Online");
    
    for (const auto& n : gState_p->second.onlineUsers_) {
        ImGui::Text(n.c_str());
    }

    ImGui::EndChild();
}

void bottomPane(const std::unique_ptr<std::pair<GuiState, SyncedState>>& gState_p) {

    ImGui::BeginChild("BottomWindow", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), false);

    InputTextWrapper messageBox{150, "##", true};
    messageBox.Render(gState_p->first.inputs.find("text")->second);

    ImGui::SameLine();

    if (ImGui::Button("Send Message")) {
        gState_p->first.writeMessage(gState_p->first.inputs.find("text")->second);
        gState_p->first.inputs.find("text")->second.clear();
    };

    ImGui::PushStyleColor(ImGuiCol_Button, ImColor(255, 0, 0, 255).Value);
    if (ImGui::Button("Leave")) {
        gState_p->first.client_->send({ gState_p->second.localUsername , shriller::netv2::MessageType::DISCONNECT});
        std::exit(1);  // NOLINT(concurrency-mt-unsafe)
    }
    ImGui::PopStyleColor();

    ImGui::EndChild();
}

void renderUI(const std::unique_ptr<std::pair<GuiState, SyncedState>>& gState_p) {
   
  
    switch (gState_p->first.currentView) {

    case GuiState::View::menu: {
        ImGui::Begin("Main Menu", nullptr);

        InputTextWrapper username{ 150, "Username", true };

        username.Render(gState_p->first.inputs.find("username")->second);
        if (ImGui::Button("Connect")) {
            gState_p->first.makeConnection("127.0.0.1", 60000);
        }

        ImGui::End();
        break;
    }

    case GuiState::View::chat:

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowSize({ io.DisplaySize.x , io.DisplaySize.y });
        ImGui::Begin("Main Window", nullptr, flags::main);
        topPane(gState_p);
        bottomPane(gState_p);
        ImGui::End();
    }
}


void clientProcessor(std::unique_ptr<std::pair<GuiState, SyncedState>>& gState_p, clientThreads& cThreads) {
    cThreads.main = std::jthread{[&]() {
          cThreads.read = std::jthread([&]() {
              
              for (;;) {

	              if (auto value_ = gState_p->first.client_->read(); value_.value().index() != 0) {
                     
 
                      struct visitor {

                          std::unique_ptr<std::pair<GuiState, SyncedState>>& gState_pv;

                          void operator() (const shriller::netv2::message& mes) const
                          {
                              switch (mes.type) {
                              case shriller::netv2::MessageType::DIRECT_MESSAGE:
                                  std::cout << "[ " << mes.username << " ]: " << mes.content << std::endl;
                                  break;
                              case shriller::netv2::MessageType::BROADCAST_MESSAGE: break;
                              case shriller::netv2::MessageType::DISCONNECT: break;
                              case shriller::netv2::MessageType::FIRST_CONNECT: break;
                              default: ;
                              }                       

                          }

                          void operator()(const shriller::netv2::ChatState& cs) const{
                             
                             gState_pv->second.onlineUsers_.clear();

                             for (const auto& [username, message, ignore] : cs.messageHistory) {

                                // std::cout << std::format("ignore: {}", ignore) << '\n';

                                 if (auto osEnd = gState_pv->second.messageHistory.end();
	                                 (std::find(gState_pv->second.messageHistory.begin(),
	                                            osEnd
	                                            , std::make_tuple(username,message)) == osEnd) && (gState_pv->second.localUsername != ignore)
                                     ) {
                                     gState_pv->second.messageHistory.emplace_back(std::tuple(username, message));
                                 }
                             }
                             for (const auto& [port, username] : cs.onlineUsers) {
	                             if (auto osEnd = gState_pv->second.onlineUsers_.end();
		                             std::find(gState_pv->second.onlineUsers_.begin(),
		                                       osEnd
		                                       , username) == osEnd
                                     ) {
                                     gState_pv->second.onlineUsers_.emplace_back(username);
                                 }
                                 
                             }
                          }

                          void operator()(const shriller::netv2::ownedMessage& om) const {
                             
                          }
                          void operator()(std::monostate ms) const {}

                      };

                      visitor vs { gState_p };
                      

                      std::visit(vs, value_.value());
                  }
              } 
          }); 

          cThreads.write = std::jthread([&]() {
              for (;;){
                  
                  std::string message = gState_p->first.readMessage();
                 
                  if (gState_p->first.connected) {

                      if (!gState_p->first.init) {
                          std::cout << "sending first connection" << std::endl;
                          auto username = gState_p->first.inputs.find("username")->second;
                          gState_p->second.localUsername = username;
                          gState_p->first.client_->send({ std::move(username), shriller::netv2::MessageType::FIRST_CONNECT });
                          gState_p->first.init = true;
                      }

                      if (!message.empty()) {
                          std::map<std::string, std::string> testMap;

                          gState_p->second.messageHistory.emplace_back(std::pair{ gState_p->second.localUsername , message});
                          gState_p->first.client_->send({ 
                              .content {std::move(message)}, 
                              .type = shriller::netv2::MessageType::BROADCAST_MESSAGE , 
                          });
                      }
                  }
             }
         });
     } };
}


int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void) io;

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
    //ImFont* font2 = io.Fonts->AddFontFromFileTTF("verdana.ttf", 16.0f);

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");


    auto gState_p 
        = std::make_unique<std::pair<GuiState, SyncedState>> 
        (std::vector<std::string>{ "text", "username"}, SyncedState{});
    
    clientThreads cThreads;


    clientProcessor(gState_p, cThreads);

    
    while (!glfwWindowShouldClose(window))
    {
       
        processInput(window);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();


        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGui::DockSpaceOverViewport();
        }

        renderUI(gState_p);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
  
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if ((glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) && toggle) {
        std::cout << "you pressed q " << std::endl;
        toggle = false;
        return;
    }
    toggle = true;
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}