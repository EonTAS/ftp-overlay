#define TESLA_INIT_IMPL // If you have more than one file using the tesla header, only define this in the main one

#include <tesla.hpp> // The Tesla Header
#include "core/server.hpp"

#define SERVER_PORT 1234

class DvrOverlay : public tsl::Gui
{
private:
    nxgallery::core::CWebServer *webServer;
    char serverString[30];

public:
    DvrOverlay(nxgallery::core::CWebServer *webServer)
    {
        this->webServer = webServer;
    }

    // Called when this Gui gets loaded to create the UI
    // Allocate all elements on the heap. libtesla will make sure to clean them up when not needed anymore
    virtual tsl::elm::Element *createUI() override
    {
        // A OverlayFrame is the base element every overlay consists of. This will draw the default Title and Subtitle.
        // If you need more information in the header or want to change it's look, use a HeaderOverlayFrame.
        auto frame = new tsl::elm::OverlayFrame(APP_TITLE, APP_VERSION);

        // A list that can contain sub elements and handles scrolling
        auto list = new tsl::elm::List();

        webServer->GetAddress(serverString);
        auto infodrawer = new tsl::elm::CustomDrawer(
            [this](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h)
            {
                renderer->drawString("Server-Address:", false, x + 3, y + 20, 16, renderer->a(0xFFFF));
                renderer->drawString(serverString, false, x + 130, y + 20, 16, renderer->a(0xFFFF));
            });

        list->addItem(infodrawer, 70);

        //// Add the list to the frame for it to be drawn
        frame->setContent(list);

        // Return the frame to have it become the top level element of this Gui
        return frame;
    }

    // Called once every frame to update values
    virtual void update() override
    {
        webServer->ServeLoop();
    }

    // Called once every frame to handle inputs not handled by other UI elements
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override
    {
        return false; // Return true here to signal the inputs have been consumed
    }
};

class OverlayTest : public tsl::Overlay
{
private:
    nxgallery::core::CWebServer *webServer;

public:
    // libtesla already initialized fs, hid, pl, pmdmnt, hid:sys and set:sys
    virtual void initServices() override
    {
        nifmInitialize(NifmServiceType_User);
        socketInitializeDefault();
        fsdevMountSdmc();
    }
    virtual void exitServices() override
    {
        webServer->Stop();
        nifmExit();
        socketExit();
        fsdevUnmountDevice("sdmc");
    }

    virtual void onShow() override {} // Called before overlay wants to change from invisible to visible state
    virtual void onHide() override {} // Called before overlay wants to change from visible to invisible state

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override
    {
        if (webServer)
        {
            webServer->Stop();
        }
        webServer = new nxgallery::core::CWebServer(SERVER_PORT);
        webServer->AddMountPoint("sdmc:/switch/PersonalServer");
        webServer->Start();
        return initially<DvrOverlay>(webServer); // Initial Gui to load. It's possible to pass arguments to it's constructor like this
    }
};

int main(int argc, char **argv)
{
    tsl::loop<OverlayTest>(argc, argv);
}