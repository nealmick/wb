#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "imgui.h"
#include <GL/glew.h>



class Browser {
public:
    Browser();
    void DrawUI();
    
private:
    void FetchURL(const std::string& url, bool addToHistory);
    void RenderHTMLContent();
    void ParseBasicHTML(const std::string& html);
    void LoadImageTexture(const std::string& url);
    
    char m_urlInput[1024] = "https://news.ycombinator.com";
    std::string m_pageContent;
    
    struct HTMLNode {
        std::string tag;
        std::string text;
        std::map<std::string, std::string> attrs;
        std::vector<HTMLNode> children;
    };
    
    HTMLNode m_domRoot;
	struct TextureData {
	    GLuint id;
	    int width;
	    int height;
	};

	std::map<std::string, TextureData> m_textures;

    //resolve relative urls
    std::string ResolveURL(const std::string& base, const std::string& relative);


    //url history, foward, and back...
    std::vector<std::string> m_history;
    int m_historyPos = -1;
    void NavigateBack();
    void NavigateForward();



};