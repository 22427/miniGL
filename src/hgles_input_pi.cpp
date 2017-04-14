#ifdef HGLES_USE_PI
#include <hgles_input_pi.h>
#include <hgles_log.h>
#include <cstdio>
#include <stdlib.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <dirent.h>
#include <hgles_window.h>
#include <fstream>

#include <sstream>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <algorithm>

namespace idev {
#define IDEV_KEYBOARD_EVENTS "120013"
#define IDEV_MOUSE_EVENTS "17"
#define IDEV_B_EVENTS "EV"
typedef std::string String;
class Device
{
public:
	//I: ...
	struct I_data
	{
		//Bus=<Bus>
		String Bus;
		//Vendor=<Vendor>
		String Vendor;
		//Product=<Product>
		String Product;
		//Version=<Version>
		String Version;
	} I;
	//N: Name=<N>
	String N;
	//P: Phys=<P>
	String P;
	//S: Sysfs=<S>
	String S;
	//U: Uniq=<U>
	String U;
	//H: Handlers=<H>
	std::vector<String> H;
	//B: <first>=<second>
	std::unordered_map<String,String> B;

	// parse the s**t out of it. Not the most reliable parser, not even close,
	// but it works for now!
	// returns true if there is more to read!
	bool read(std::ifstream& f);


	// searches for an handler starting with <starts_with> and returns it.
	// if none is found an empty string is returned.
	String get_handler_starting_with(const String& starts_with) const
	{
		for(const auto& h : H)
		{
			if(h.find_first_of(starts_with) == 0)
				return h;
		}
		return "";
	}
};


//filters device list, searching for devices where the "B" attribute attrib
//matches value
std::vector<Device> filter_B(const std::vector<Device>& in,
							 const String& attrib,
							 const String& value)
{
	std::vector<Device> out;
	for(const auto& d : in)
	{
		auto it = d.B.find(attrib);
		if(it != d.B.end() && (*it).second == value)
			out.push_back(d);
	}
	return out;
}

// reads all devices from proc/bus/input/devices
std::vector<Device> get_devices(const String& p="/proc/bus/input/devices")
{
	std::ifstream f;
	f.open(p);
	if(!f.is_open())
		std::cout<<"cannot open /proc/bus/input/devices"<<std::endl;
	std::vector<Device> res;
	Device dev;
	dev.B.clear();
	dev.H.clear();
	while(dev.read(f))
	{
		res.push_back(dev);
		dev.B.clear();
		dev.H.clear();
	}

	f.close();
	return  res;
}

std::vector<Device> get_keyboards(const String& p="/proc/bus/input/devices")
{
	auto devs = get_devices(p);
	return  filter_B(devs,IDEV_B_EVENTS,IDEV_KEYBOARD_EVENTS);
}

std::vector<Device> get_mice(const String& p = "/proc/bus/input/devices")
{
	auto devs = get_devices(p);
	return  filter_B(devs,IDEV_B_EVENTS,IDEV_MOUSE_EVENTS);
}



}


namespace hgles
{
void InputSystem::turn_on_keyboards()
{
	turn_off_keyboards();
	auto kb = idev::get_keyboards();
	pollfd pfd;
	// prepare input devices
	for(const auto& d : kb)
	{
		auto e = d.get_handler_starting_with("event");
		if(!e.empty())
		{
			pfd.fd = open(e.c_str(),O_RDONLY);
			pfd.events = POLLIN | POLLPRI;
			m_observed_keyboards.push_back(pfd);
		}
	}
}

void InputSystem::turn_off_keyboards()
{

	for(const auto& fd : m_observed_keyboards)
	{
		close(fd.fd);
	}
	m_observed_keyboards.clear();
}


void InputSystem::turn_on_mice()
{
	turn_off_mice();
	auto kb = idev::get_mice();
	pollfd pfd;
	// prepare input devices
	for(const auto& d : kb)
	{
		auto e = d.get_handler_starting_with("event");
		if(!e.empty())
		{
			pfd.fd = open(e.c_str(),O_RDONLY);
			pfd.events = POLLIN | POLLPRI;
			m_observed_mice.push_back(pfd);
		}
	}
}

void InputSystem::turn_off_mice()
{

	for(const auto& fd : m_observed_mice)
	{
		close(fd.fd);
	}
	m_observed_mice.clear();
}


InputSystem::InputSystem()
{
	memset(m_key_state,0,K_LAST+1);
	memset(m_button_state,0,BUTTON_LAST+1);

	memset(m_cursor_position,0,2*sizeof (int));
	memset(m_wheel_position,0,2*sizeof (int));


}

void InputSystem::init(Window *w)
{
	m_cursor_position[0] = w->m_win_sze.x/2;
	m_cursor_position[1] = w->m_win_sze.y/2;
}

void InputSystem::add_keyboard_listener(KeyboardListener *kbl)
{
	if(m_keyboard_listener.empty())
	{
		turn_on_keyboards();
	}
	m_keyboard_listener.push_back(kbl);
}


void InputSystem::remove_keyboard_listener(KeyboardListener *kbl)
{
	m_keyboard_listener.erase(std::find(m_keyboard_listener.begin(),
										m_keyboard_listener.end(),kbl));
	if(m_keyboard_listener.empty())
	{
		turn_off_keyboards();
	}
}

void InputSystem::add_mouse_listener(MouseListener *ml)
{
	if(m_mouse_listener.empty())
	{
		turn_on_mice();
	}
	m_mouse_listener.push_back(ml);
}


void InputSystem::remove_mouse_listener(MouseListener *ml)
{
	m_mouse_listener.erase(std::find(m_mouse_listener.begin(),
									 m_mouse_listener.end(),ml));
	if(m_mouse_listener.empty())
	{
		turn_off_mice();
	}
}




void InputSystem::poll_events()
{
	struct input_event ie[32];
	bzero(&ie, sizeof (input_event));
	if(!m_keyboard_listener.empty())
	{
		// check for keyboard input:
		auto pres =poll(m_observed_keyboards.data(),m_observed_keyboards.size(),0);

		if(pres < 0)
		{
			ERROR("poll failed %d",errno);
		}
		// handle input
		if(pres > 0)
			for(const auto& k : m_observed_keyboards)
			{
				auto r = read(k.fd,&ie,sizeof(ie));
				r = r/sizeof (input_event);
				for(int i = 0; i< r;i++)
				{
					auto& e = ie[i];
					if(e.type != EV_KEY)
						continue;
					Key k = (Key)e.code; // key code!
					//e.value; // 0 release, 1 keypress and 2 autorepeat
					if(e.value == 0)
					{
						m_key_state[k] = false;
						for(auto& l :m_keyboard_listener) l->key_up(k);
					}
					else if(e.value == 1)
					{
						m_key_state[k] = true;
						for(auto& l :m_keyboard_listener) l->key_down(k);
					}
					else if(e.value == 2)
					{
						for(auto& l :m_keyboard_listener) l->key_repeat(k);
					}
				}
			}
	}

	bzero(&ie, sizeof (input_event));
	if(!m_mouse_listener.empty())
	{
		// check for keyboard input:
		auto pres =poll(m_observed_mice.data(),m_observed_mice.size(),0);

		if(pres < 0)
		{
			ERROR("poll failed %d",errno);
		}


		// handle input
		if(pres > 0)
			for(const auto& m : m_observed_mice)
			{
				auto r = read(m.fd,&ie,sizeof(ie));
				r = r/sizeof (input_event);
				for(int i = 0; i< r;i++)
				{
					auto& e = ie[i];
					if(e.type == EV_REL)
					{
						if(e.code == REL_X)
						{
							m_cursor_position[0] += e.value;
						}
						else if(e.code == REL_Y)
						{
							m_cursor_position[1] += e.value;
						}
						if(e.code == REL_WHEEL)
						{
							m_wheel_position[0] += e.value;
						}
						else if(e.code == REL_HWHEEL)
						{
							m_wheel_position[1] += e.value;
						}
					}
					else if(e.type == EV_KEY)
					{
						Button btn = (Button)(e.code & 0xFF);
						if(e.value == 0)
						{
							m_button_state[btn]=false;
							for(auto& l :m_mouse_listener)l->button_up(btn);
						}
						else if(e.value == 1)
						{
							m_button_state[btn]=true;
							for(auto& l :m_mouse_listener)l->button_down(btn);
						}
						else if(e.value == 2)
						{
							// not interesting !
						}
					}

				}
			}
		for(auto& l :m_mouse_listener)l->cursor(m_cursor_position[0],m_cursor_position[1]);
		for(auto& l :m_mouse_listener)l->scroll(m_wheel_position[0],m_wheel_position[1]);
	}
}



}



bool idev::Device::read(std::ifstream &f)
{
	String line;
	while(std::getline(f,line))
	{
		if(line[0] == 'I')
		{
			line = line.substr(line.find_first_of('=')+1);
			I.Bus = line.substr(0,line.find_first_of(' '));
			line = line.substr(line.find_first_of('=')+1);
			I.Vendor = line.substr(0,line.find_first_of(' '));
			line = line.substr(line.find_first_of('=')+1);
			I.Product = line.substr(0,line.find_first_of(' '));
			line = line.substr(line.find_first_of('=')+1);
			I.Version = line.substr(0,line.find_first_of(' '));
		}
		else if(line[0] == 'N')
		{
			line = line.substr(line.find_first_of('=')+1);
			N = line;
		}
		else if(line[0] == 'P')
		{
			line = line.substr(line.find_first_of('=')+1);
			P = line;
		}
		else if(line[0] == 'S')
		{
			line = line.substr(line.find_first_of('=')+1);
			S = line;
		}
		else if(line[0] == 'U')
		{
			line = line.substr(line.find_first_of('=')+1);
			U = line;
		}
		else if(line[0] == 'H')
		{
			line = line.substr(line.find_first_of('=')+1);
			std::stringstream ss(line);
			String h;
			while(getline(ss,h,' '))
			{
				H.push_back(h);
			}
		}
		else if(line[0] == 'B')
		{
			line = line.substr(line.find_first_of(' ')+1);
			String name = line.substr(0,line.find_first_of('='));
			String valu = line.substr(line.find_first_of('=')+1);
			B[name] = valu;
		}
		else
			break;
	}
	return !f.eof();
}

#endif
