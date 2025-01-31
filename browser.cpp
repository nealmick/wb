#include "browser.h"
#include "imgui.h"
#include <curl/curl.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GL/glew.h>  
#include <iostream>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
Browser::Browser() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    // Initialize with resolved URL
    std::string initialUrl = ResolveURL("https://news.ycombinator.com", m_urlInput);
    strncpy(m_urlInput, initialUrl.c_str(), sizeof(m_urlInput));
    m_history.push_back(initialUrl);
    m_historyPos = 0;
    FetchURL(initialUrl, true);
}

void Browser::FetchURL(const std::string& url, bool addToHistory = true) {
    // Clear old textures
    for (auto& [key, tex] : m_textures) {
        glDeleteTextures(1, &tex.id);
    }
    m_textures.clear();
    std::string resolvedUrl = ResolveURL(m_urlInput, url);
    
    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, resolvedUrl.c_str()); // FIXED LINE
        CURLcode res;
        std::string readBuffer;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "SimpleBrowser/1.0");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        res = curl_easy_perform(curl);
        if(res == CURLE_OK) {
            m_pageContent = readBuffer;
        } else {
            m_pageContent = "Failed to fetch URL: " + std::string(curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
    }
    if (addToHistory) {
        // Trim future history if we're not at the end
        if (m_historyPos < static_cast<int>(m_history.size()-1)) {
            m_history.erase(m_history.begin()+m_historyPos+1, m_history.end());
        }
        m_history.push_back(url);
        m_historyPos = m_history.size()-1;
    }

    // Update URL bar text
    strncpy(m_urlInput, resolvedUrl.c_str(), sizeof(m_urlInput));
    m_urlInput[sizeof(m_urlInput)-1] = '\0'; // Ensure null termination
}

void Browser::DrawUI() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // Set window to fill entire viewport
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 8)); // More vertical spacing
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    
    if (ImGui::Begin("MainBrowser", nullptr, flags)) {
        // Navigation controls
        ImGui::BeginGroup();
        {
            // Back/Forward buttons
            const float buttonSize = ImGui::GetFrameHeight();
            
            // Back button
            bool canGoBack = (m_historyPos > 0);
            if (!canGoBack) ImGui::BeginDisabled();
            if (ImGui::Button("<", ImVec2(buttonSize, 0))) {
                NavigateBack();
            }
            if (!canGoBack) ImGui::EndDisabled();

            ImGui::SameLine();

            // Forward button
            bool canGoForward = (m_historyPos < static_cast<int>(m_history.size())-1);
            if (!canGoForward) ImGui::BeginDisabled();
            if (ImGui::Button(">", ImVec2(buttonSize, 0))) {
                NavigateForward();
            }
            if (!canGoForward) ImGui::EndDisabled();

            ImGui::SameLine();

            // URL input and Go button
            float availableWidth = ImGui::GetContentRegionAvail().x - 50.0f;
            ImGui::PushItemWidth(availableWidth);
            if (ImGui::InputText("##URL", m_urlInput, IM_ARRAYSIZE(m_urlInput), 
                               ImGuiInputTextFlags_EnterReturnsTrue)) {
                FetchURL(m_urlInput);
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Go", ImVec2(50.0f, 0))) {
                FetchURL(m_urlInput);
            }
        }
        ImGui::EndGroup();


        // Content area (fills remaining space)
        ImGui::BeginChild("Content", 
                     ImVec2(0, ImGui::GetContentRegionAvail().y), 
                     ImGuiChildFlags_Border, 
                     ImGuiWindowFlags_HorizontalScrollbar);
    
	    RenderHTMLContent();
	    
	    ImGui::EndChild();


    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void Browser::ParseBasicHTML(const std::string& html) {
    m_domRoot = HTMLNode{"root", "", {}, {}};
    std::vector<HTMLNode*> stack = {&m_domRoot};
    size_t pos = 0;
    
    while (pos < html.size()) {
        if (html[pos] == '<') {
            size_t tag_start = pos + 1;
            size_t tag_end = html.find('>', tag_start);
            if (tag_end == std::string::npos) break;
            
            std::string tag_content = html.substr(tag_start, tag_end - tag_start);
            pos = tag_end + 1;
            
            if (tag_content[0] == '/') {
                if (!stack.empty()) stack.pop_back();
            } 
            else {
                bool self_closing = false;
                if (!tag_content.empty() && tag_content.back() == '/') {
                    self_closing = true;
                    tag_content.pop_back();
                }

                HTMLNode node;
                size_t space_pos = tag_content.find(' ');
                node.tag = tag_content.substr(0, space_pos);
                
                // Parse attributes
                size_t attr_start = space_pos;
                while (attr_start != std::string::npos) {
                    attr_start = tag_content.find_first_not_of(' ', attr_start);
                    if (attr_start == std::string::npos) break;
                    
                    size_t eq_pos = tag_content.find('=', attr_start);
                    if (eq_pos == std::string::npos) break;
                    
                    std::string key = tag_content.substr(attr_start, eq_pos - attr_start);
                    attr_start = eq_pos + 1;
                    
                    if (attr_start < tag_content.size() && tag_content[attr_start] == '"') {
                        size_t value_start = attr_start + 1;
                        size_t value_end = tag_content.find('"', value_start);
                        if (value_end != std::string::npos) {
                            node.attrs[key] = tag_content.substr(value_start, value_end - value_start);
                            attr_start = value_end + 1;
                        }
                    }
                }
                
                stack.back()->children.push_back(node);
                if (!self_closing) {
                    stack.push_back(&stack.back()->children.back());
                }
            }
        } 
        else {
            size_t text_end = html.find('<', pos);
            if (text_end == std::string::npos) text_end = html.size();
            
            std::string text = html.substr(pos, text_end - pos);
            text.erase(text.find_last_not_of(" \n\r\t") + 1);
            text.erase(0, text.find_first_not_of(" \n\r\t"));
            
            // PREVENT EMPTY TEXT NODES
            if (!text.empty()) {
                stack.back()->children.push_back(HTMLNode{"text", text, {}, {}});
            }
            pos = text_end;
        }
    }
}

void Browser::LoadImageTexture(const std::string& url) {
    if (url.find("://") == std::string::npos) {
        std::cerr << "[Image] Invalid URL: " << url << std::endl;
        return;
    }
    if (m_textures.count(url)) return;
    // Add format check here
    size_t dot_pos = url.find_last_of(".");
    if (dot_pos == std::string::npos) return;
    std::string extension = url.substr(dot_pos + 1);
    if (extension == "svg" || extension == "gif") {
        std::cerr << "[Texture] Skipping unsupported format: " << url << std::endl;
        return;
    }
    CURL* curl = curl_easy_init();
    if (!curl) return;

    std::string image_data;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &image_data);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // ADDED REDIRECT FOLLOWING
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        return;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);

    if (response_code != 200) {  // ENSURE ONLY 200 OK
        std::cerr << "[HTTP] Non-200 response: " << response_code << std::endl;
        return;
    }
    if (image_data.empty()) {
        std::cerr << "[Image] Empty data received for " << url << std::endl;
        return;
    }

    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;    
    unsigned char* data = stbi_load_from_memory(
        reinterpret_cast<const unsigned char*>(image_data.data()),
        image_data.size(),
        &width, &height, &channels,
        4  // Force 4 channels (RGBA)
    );

    if (!data) {
        std::cerr << "[STB] Load failed: " << stbi_failure_reason() 
                  << " (" << url << ")" << std::endl;
        return;
    }

    if (width <= 0 || height <= 0) {
        std::cerr << "[Image] Invalid dimensions: " 
                  << width << "x" << height << " for " << url << std::endl;
        stbi_image_free(data);
        return;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, 
                GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[OpenGL] Error 0x" << std::hex << err 
                  << " when creating texture for " << url << std::endl;
        glDeleteTextures(1, &texture);
        stbi_image_free(data);
        return;
    }

    m_textures[url] = {texture, width, height};
    std::cout << "[Texture] Successfully loaded: " << url 
              << " (" << width << "x" << height << ")" << std::endl;
    
    stbi_image_free(data);
}
void Browser::RenderHTMLContent() {
    static std::string lastContent;
    
    if (m_pageContent != lastContent) {
        ParseBasicHTML(m_pageContent);
        lastContent = m_pageContent;
        
        std::function<void(const HTMLNode&)> preloadImages = [&](const HTMLNode& node) {
            if (node.tag == "img" && node.attrs.count("src")) {
                std::string resolved = ResolveURL(m_urlInput, node.attrs.at("src"));
                if (!resolved.empty()) {
                    LoadImageTexture(resolved);
                }
            }
            for (const auto& child : node.children) {
                preloadImages(child);
            }
        };
        preloadImages(m_domRoot);
    }

    std::function<void(const HTMLNode&)> renderNode = [&](const HTMLNode& node) {
            if (node.tag == "text") {
                if (!node.text.empty()) {
                    // Render text nodes inline
                    ImGui::SameLine(0, 0);
                    ImGui::TextUnformatted(node.text.c_str());
                    ImGui::SameLine(0, 0);
                }
                return;
            }
            else if (node.tag == "p") {
                ImGui::PushTextWrapPos();
                bool firstChild = true;
                for (const auto& child : node.children) {
                    if (!firstChild) ImGui::SameLine(0, 0);
                    renderNode(child);
                    firstChild = false;
                }
                ImGui::PopTextWrapPos();
                ImGui::NewLine();
            }
            
                            
            else if (node.tag == "h1") {
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                // Render all children (including text nodes)
                for (const auto& child : node.children) {
                    renderNode(child);
                }
                ImGui::PopFont();
                ImGui::Separator();
            }
            else if (node.tag == "h2") {
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                for (const auto& child : node.children) {
                    renderNode(child);
                }
                ImGui::PopFont();
            }
            else if (node.tag == "a" && node.attrs.count("href")) {
                std::string linkText;
                for (const auto& child : node.children) {
                    if (child.tag == "text") {
                        linkText += child.text;
                    }
                }
                if(!linkText.empty()) {
                    ImGui::PushID(node.attrs.at("href").c_str()); // Unique ID based on URL
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 255, 255));
                    if (ImGui::Selectable(linkText.c_str())) {
                        std::string resolved = ResolveURL(m_urlInput, node.attrs.at("href"));
                        FetchURL(resolved);
                    }
                    // Add underline
                    ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddLine(
                        ImVec2(min.x, max.y), 
                        ImVec2(max.x, max.y), 
                        IM_COL32(0, 0, 255, 255)
                    );
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }
               
            }
            else if (node.tag == "p") {
                ImGui::PushTextWrapPos();
                for (const auto& child : node.children) renderNode(child);
                ImGui::PopTextWrapPos();
                ImGui::Spacing();
            }
           
            
                
            else if (node.tag == "img" && node.attrs.count("src")) {
                const std::string& src = node.attrs.at("src");
                if (m_textures.count(src)) {
                    const TextureData& tex = m_textures[src];
                    ImGui::Image(
                        (ImTextureID)(static_cast<uint64_t>(tex.id)),  // Correct cast
                        ImVec2(tex.width, tex.height),
                        ImVec2(0,0),
                        ImVec2(1,1),
                        ImVec4(1,1,1,1),
                        ImVec4(0,0,0,0)
                    );
                } else {
                    ImGui::TextColored(ImVec4(1,0,0,1), "[Loading: %s]", src.c_str());
                }
            }
            else if (node.tag == "b" || node.tag == "strong") {
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
                for (const auto& child : node.children) renderNode(child);
                ImGui::PopFont();
            }
            else if (node.tag == "i" || node.tag == "em") {
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[3]);
                for (const auto& child : node.children) renderNode(child);
                ImGui::PopFont();
            }
            else {
                for (const auto& child : node.children) {
                    renderNode(child);
                }
            }
        };
        
        renderNode(m_domRoot);
}
std::string Browser::ResolveURL(const std::string& base, const std::string& relative) {
    if (relative.empty()) return "";
    if (relative.find("://") != std::string::npos) return relative;

    // Handle protocol-relative URLs (//example.com)
    if (relative.substr(0, 2) == "//") {
        size_t protocol_end = base.find("://");
        if (protocol_end != std::string::npos) {
            return base.substr(0, protocol_end + 1) + relative.substr(2);
        }
        return "https:" + relative;
    }

    size_t protocol_pos = base.find("://");
    if (protocol_pos == std::string::npos) return relative;

    std::string protocol = base.substr(0, protocol_pos + 3);
    std::string base_path = base.substr(protocol_pos + 3);

    // Handle absolute path references
    if (relative[0] == '/') {
        size_t domain_end = base_path.find('/');
        if (domain_end == std::string::npos) return protocol + base_path + relative;
        return protocol + base_path.substr(0, domain_end) + relative;
    }

    // Handle relative paths
    std::string path = base;
    size_t last_slash = path.rfind('/');
    if (last_slash == std::string::npos || last_slash < protocol_pos + 3) {
        path += "/";
    } else {
        path = path.substr(0, last_slash + 1);
    }
    
    return path + relative;
}

void Browser::NavigateBack() {
    if (m_historyPos > 0) {
        m_historyPos--;
        FetchURL(m_history[m_historyPos], false);
    }
}

void Browser::NavigateForward() {
    if (m_historyPos < static_cast<int>(m_history.size())-1) {
        m_historyPos++;
        FetchURL(m_history[m_historyPos], false);
    }
}


