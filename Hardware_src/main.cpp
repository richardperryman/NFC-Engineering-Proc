
#include <vector>
#include <chrono>
#include <thread>
#include <sys/stat.h>
#include <Types.h>
#include <Serial.h>
#include <GPIOPin.h>
#include <DecodedPacket.h>
#include <EncodedPacket.h>
#include <ServerConnection.h>
#include <AuthenticationModule.h>
#include <Debug.h>

// To do:
// Read configuration file at start up (server URL, lock ID)

// For QR:
// User takes photo of code (the code is the lock ID), sends code to server
// Server forwards user info to Pi (server is a authentication module)

static uint32_t LOCK_ID;
static std::string SERVER_URL;

static GPIOPin* GREEN;
static GPIOPin* BLUE;
static GPIOPin* RED;
// static GPIOPin* RELAY_SIGNAL;

static void maintenanceLoop();
static void pollingLoop(std::vector<AuthenticationModule*>* modules, ServerConnection* connection);
static int8_t readConfiguration(std::string filepath);
static int8_t verifyModules(std::vector<AuthenticationModule*>* modules);

static void setUpLEDs()
{
	GREEN = new GPIOPin("2");
	BLUE = new GPIOPin("3");
	RED = new GPIOPin("4");
	
	GREEN->exportPin();
	BLUE->exportPin();
	RED->exportPin();
	
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	
	GREEN->setOutput();
	BLUE->setOutput();
	RED->setOutput();
	
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

static void teardown()
{
	GREEN->unexportPin();
	BLUE->unexportPin();
	RED->unexportPin();	

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    delete(GREEN);
    delete(BLUE);
    delete(RED);
}

void accessGranted()
{
    DEBUG_LOG(INFO, __FUNCTION__, "Access granted.");
    BLUE->setValue(GPIO_LOW);
    GREEN->setValue(GPIO_HIGH);
    // RELAY_SIGNAL->setValue(GPIO_HIGH);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // Disengage the lock for three seconds
    
    BLUE->setValue(GPIO_HIGH);
    GREEN->setValue(GPIO_LOW);
    // RELAY_SIGNAL->setValue(GPIO_LOW);
}

void accessDenied()
{
    DEBUG_LOG(INFO, __FUNCTION__, "Access denied.");
    BLUE->setValue(GPIO_LOW);
    RED->setValue(GPIO_HIGH);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // Show red for two seconds
    
    BLUE->setValue(GPIO_HIGH);
    RED->setValue(GPIO_LOW);
}

int main()
{
    uint8_t status = 0;
    std::string filepath = "/not/real/rightnow.cfg";
    std::vector<AuthenticationModule*> modules;

    setUpLEDs();
    status = readConfiguration(filepath);
    
    if (status != 0) {
        DEBUG_LOG(CRITICAL, __FUNCTION__, "Failure to read configuration from %s", filepath.c_str());
        maintenanceLoop();
    } else {
        ServerConnection* connection = new ServerConnection(SERVER_URL);
        connection->openConnection();
        status = connection->verifyConnection();
        
        if (status != 0) {
            DEBUG_LOG(CRITICAL, __FUNCTION__, "Cannot reach server.");
            maintenanceLoop();
        } else {
            status = verifyModules(&modules);
            
            if (status != 0) {
                DEBUG_LOG(CRITICAL, __FUNCTION__, "Error retrieving authentication modules.");
            } else if (modules.size() == 0) {
                DEBUG_LOG(CRITICAL, __FUNCTION__, "No authentication modules found!");
                maintenanceLoop();
            } else {
                pollingLoop(&modules, connection);
            }
        }
        
        connection->closeConnection();
    }
    
    teardown();
    DEBUG_LOG(INFO, __FUNCTION__, "Shut down successful.");
    return 0;
}

static int8_t readConfiguration(std::string filepath)
{
    LOCK_ID = 0x1;
    SERVER_URL = "http://sbacs.48tdba4fch.us-west-2.elasticbeanstalk.com";
    //SERVER_URL = "127.0.0.1:3000";
    
	return 0;
}

static void maintenanceLoop()
{
    while(true)
    {
        RED->setValue(GPIO_HIGH);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        RED->setValue(GPIO_LOW);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

static void pollingLoop(std::vector<AuthenticationModule*>* modules, ServerConnection* connection)
{
    while(true) // TODO: Switch this loop to check for a kill file so I can quit gracefully
    {
        uint16_t counter = 50;
        bool counting = false;
        uint16_t tokenCount = 0;
        
        // Poll each device
        for (uint8_t i = 0; i < modules->size(); i++)
        {
            AuthenticationModule* module = modules->at(i);
            printf("-- Polling device %s\n", module->getID());
            
            if (module->getToken())
            {
                tokenCount++;
                            
                // On first token, start 5-second countdown
                if (!counting)
                {
                    counting = true;
                }         
            }
            
            if (tokenCount == modules->size() || counter == 0)
            {      
                // Attempt an unlock
                if (0 == connection->requestAccess(LOCK_ID, modules)) {
                    accessGranted();
                } else {
                    accessDenied();
                }

                counting = false;
                counter = 50;
                tokenCount = 0;
            }
            
            if (counting) counter--;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }   
}

static int8_t verifyModules(std::vector<AuthenticationModule*>* modules)
{
    std::vector<std::string> device_paths; // Not every ACMx device is a module

    // Get all device paths on /dev/ttyACM*
    FILE * f = popen( "find /dev -name ttyACM*", "r" );
    if ( f == 0 ) {
        DEBUG_LOG(CRITICAL, __FUNCTION__, "Could not get device listing from /dev.");
        return -1;
    }
    
    const int BUFSIZE = 260; // 255 + 5 chars for "/dev/"
    char buf[ BUFSIZE ];
    
    while( fgets( buf, BUFSIZE,  f ) ) {
        std::string* path= new std::string(buf);
        device_paths.push_back(path->substr(0, path->length()-1));
        DEBUG_LOG(INFO, __FUNCTION__, "Found device on %s", (path->substr(0, path->length()-1)).c_str());
    }
    pclose( f );
    
    if (device_paths.size() == 0)
    {
        DEBUG_LOG(WARNING, __FUNCTION__, "No device paths found.");
        return -1;
    }
    
	// Get String names from modules
	EncodedPacket* setup = new EncodedPacket(OPCODE_SETUP, 0x0000);
    
    for (uint8_t i = 0; i < device_paths.size(); i++)
    {
        DEBUG_LOG(INFO, __FUNCTION__, "Sending setup packet");
        Serial *usb = new Serial(device_paths[i]);
        usb->openPort(SERIAL_BAUD_NFC);

        usb->sendPacket(*setup);

        DecodedPacket* response = usb->receivePacket();
        if (response == nullptr)
        {
            DEBUG_LOG(WARNING, __FUNCTION__, "Device on %s is not a valid authentication module.", device_paths.at(i).c_str());
            usb->closePort();
            delete(usb);
        }
        else
        {
            if (response->getOpcode() != OPCODE_DATA)
            {
                if (response->getOpcode() == OPCODE_ERROR)
                {
                    DEBUG_LOG(ERROR, __FUNCTION__, "Device on %s sent error code %04X.", device_paths.at(i).c_str(), response->getBlockNumber());
                }
                else
                {
                    DEBUG_LOG(ERROR, __FUNCTION__, "Device on %s sent a non-error, non-data packet.", device_paths.at(i).c_str());
                }
                usb->closePort();
                delete(usb);
            }
            else
            {
                uint8_t responseData[response->getDataSize()];
                response->getData(responseData);
            
                DEBUG_LOG(INFO, __FUNCTION__, "Found module with ID: %s", responseData);
                std::string* id = new std::string((const char*)responseData, response->getDataSize());
                modules->push_back(new AuthenticationModule(*id, usb));
            
                // Send ACK back
                EncodedPacket* ack = new EncodedPacket(OPCODE_ACK, response->getBlockNumber());
                usb->sendPacket(*ack);
                delete(ack);
            }
        }
    }
    
    delete(setup);
	return 0;
}
