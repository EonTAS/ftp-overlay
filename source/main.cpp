#define TESLA_INIT_IMPL // If you have more than one file using the tesla header, only define this in the main one
#include <tesla.hpp>    // The Tesla Header

class DvrOverlay : public tsl::Gui
{
private:
    char ipString[20];
    u32 ipAddress;

public:
    DvrOverlay() {}

    // Called when this Gui gets loaded to create the UI
    // Allocate all elements on the heap. libtesla will make sure to clean them up when not needed anymore
    virtual tsl::elm::Element *createUI() override
    {
        // A OverlayFrame is the base element every overlay consists of. This will draw the default Title and Subtitle.
        // If you need more information in the header or want to change it's look, use a HeaderOverlayFrame.
        auto frame = new tsl::elm::OverlayFrame(APP_TITLE, APP_VERSION);

        // A list that can contain sub elements and handles scrolling
        auto list = new tsl::elm::List();

        u32 newIp;
        nifmGetCurrentIpAddress(&newIp);
        updateIP(newIp);

        auto infodrawer = new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h)
                                                     {
            renderer->drawString("IP-Address:", false, x + 3, y + 60, 16, renderer->a(0xFFFF));
            renderer->drawString(ipString, false, x + 110, y + 60, 16, renderer->a(0xFFFF)); });

        list->addItem(infodrawer, 70);

        //// Add the list to the frame for it to be drawn
        frame->setContent(list);

        // Return the frame to have it become the top level element of this Gui
        return frame;
    }

    // Called once every frame to update values
    int currentFrame = 0;
    virtual void update() override
    {
        currentFrame++;
        // only check for dvr mode and ip cahnges every 30 fps, so 0,5-1 sec
        if (currentFrame >= 30)
        {
            currentFrame = 0;
            u32 newIp;
            nifmGetCurrentIpAddress(&newIp);
            updateIP(newIp);
        }
    }

    void updateIP(u32 newIp)
    {
        if (newIp != ipAddress)
        {
            ipAddress = newIp;
            snprintf(ipString, sizeof(ipString) - 1, "%u.%u.%u.%u", ipAddress & 0xFF, (ipAddress >> 8) & 0xFF, (ipAddress >> 16) & 0xFF, (ipAddress >> 24) & 0xFF);
        }
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
public:
    virtual void initServices() override
    {
        nifmInitialize(NifmServiceType_User);
    }
    virtual void exitServices() override
    {
        nifmExit();
    }

    virtual void onShow() override {} // Called before overlay wants to change from invisible to visible state
    virtual void onHide() override {} // Called before overlay wants to change from visible to invisible state

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override
    {
        return initially<DvrOverlay>(); // Initial Gui to load. It's possible to pass arguments to it's constructor like this
    }
};

int main(int argc, char **argv)
{
    return tsl::loop<OverlayTest>(argc, argv);
}