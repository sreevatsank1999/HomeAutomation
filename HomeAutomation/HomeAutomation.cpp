// HomeAutomation.cpp : Defines the entry point for the console application.
//
//Todo: 
//** Power Calc Function ** { Datafile Read Function; DataEntry Categorisation Fucntion; Link to Power calc Function;}
//Initiallisation Modes : FullAuto, SemiAuto, Full Manual
//Improve Repair_Config(SwitchFlags) Function
//Make initialisation of Status
//Do Something more with flags
//make Unique ID generation Function
//
//Important Debug : getline(with Delimtor) is Datafile.seekg(1, ::cur); required ?? do you need to seek one character
//				  : Once a switch is unloaded and reloaded  Datafile must continue appending data, musn't reprint the header(i.e. the Short Config data)
//				  : Streamline Data Logging Procedures, example by Add bool Enable_Log to Enable/Disable Logging For each switch
//				  : Remove timestamp from [.Data] Tag, make it part of Ini_Datafile();
//
//
//Enhancements : Attach Datafile to switch, => Datafile is Open as long as the switch is loaded and Datafile.close() in Destructor
//			   : Optimise DataRead functions by making Data Vector Static 
#include <stdio.h>
#include <algorithm>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <time.h>
#include "stdafx.h"

using namespace std;

enum Mode{Auto, Manual, Undefined = -1};
enum Type{Light, Fan, PowerSocket, Other, Undefined = -1};
enum Status{Off, On, Undefined = -1};
enum PinDir{Input, Output, Undefined = -1};
enum PinVal{Low, High, Undefined = -1};

vector<Switch> arrSwitch ;
enum Mode OperationMode = Mode::Undefined;

class GPIOClass { 

	string PinNo;
	string PinDir;
	string PinVal;
public:
	GPIOClass() {															// Default GPIOClass("4", "out", "0");
		export_PinNo("4");		   PinNo = "4";
		export_PinDir("out", "4"); PinDir = "out";
		export_PinVal("0", "4");   PinVal = "0";
	}

	~GPIOClass() {

		unexport_PinNo(PinNo);
	}

	void Initialise_Manual() {
	
		string No, Dir, Val;

		printf("Enter Pin Configuration : \n");
		printf("        GPIO Pin Number : ");												
		cin >> No;
									// First check Validity of newNo
		while (isValid_PinNo(No) == -1) {
			printf("Invalid PinNo entered......... Please reenter : ");
			cin >> No;
		}
		if (isValid_PinNo(No) == 1) {
			printf("WARNING: Make Sure this Pin is Configured in raspi_config.......\n");
			printf("Press enter key to continue....\n");
			getc(stdin);
		}

		printf("       Pin Mode(in/out) : ");
		cin >> Dir;
									// First Check validity 
		while (isValid_PinDir(Dir) == -1) {
			printf("Invalid PinMode entered......... Please reenter : ");
			cin >> Dir;
		}

		if(Dir == "out")		Val = "0";								//  Default Low		
		else					Val = "";

		PinNo = No;
		PinDir = Dir;
		PinVal = Val;									

	}
	int Initialise_Auto(ifstream &ConfigFile, string &TempBuff, string &TempVal) {
		const int GPIOAttribNo = 3;
		unsigned char GPIOFlags;

		if (TempBuff == "[Switch.GPIO]") {											 
			for (int j = 0; j < GPIOAttribNo; j++) {
				getline(ConfigFile, TempBuff, '=');
				ConfigFile.seekg(1, ios::_Seekcur);
				getline(ConfigFile, TempVal);

				if (TempBuff == "strPinNo")			PinNo = Str_deQuote(TempVal);				// undefined case not working properly
				if (TempBuff == "strPinDir")		PinDir = Str_deQuote(TempVal);
				if (TempBuff == "strPinVal")		PinVal = Str_deQuote(TempVal);
				else								break;
			}
		}
		(*this).isValid_Config(GPIOFlags);

		return GPIOFlags;
	}
	int WriteCurr_Config(ofstream &ConfigFile, bool ConfigLong) {
		const char dQuote = (char)34;

		ConfigFile << "[Switch.GPIO]\n";
		ConfigFile << "strPinNo=" << dQuote << PinNo << dQuote << "\n";
		ConfigFile << "strPinDir=" << dQuote << PinDir << dQuote << "\n";
		
		if (ConfigLong == true)	 ConfigFile << "strPinVal=" << dQuote << PinVal << dQuote << "\n";

		return 0;
	}
	bool isValid_Config(unsigned char &Setflags){																// Validity check for auto initialisation
		Setflags = 0;
		const unsigned char _NOT_VALID_PINNO = 0x04;
		const unsigned char _NOT_VALID_PINDIR = 0x02;
		const unsigned char _NOT_VALID_PINVAL = 0x01;

		if (isValid_PinNo(PinNo) == -1)				Setflags = Setflags | _NOT_VALID_PINNO;
		if (isValid_PinDir(PinDir) == -1)			Setflags = Setflags | _NOT_VALID_PINDIR;
		if (isValid_PinVal(PinVal) == -1)			Setflags = Setflags | _NOT_VALID_PINVAL;

		if ((PinDir == "in") && (PinVal == ""))		Setflags = Setflags - _NOT_VALID_PINVAL;					// Special case when PinDir == "in" and PinVal == ""
		
		if (Setflags == 0)							return true;
		else										return false;
	}
																		// Used to change the Switch to a new Pin from existing Pin (Safer Method)
	int set_PinNo(string newNo) {
		int export_Success;
		int unexport_Success;
		int ValidPinNo;
					
		ValidPinNo = isValid_PinNo(newNo);
			if (ValidPinNo == -1) {																			//****Procedure becomes Manual Because of cin !!****
			cout << "Pin No '" << newNo << "' is Invalid \n";			// Validity Check of newNo

			return (int)255;
		}
		if (ValidPinNo == 1) {
			cout<<"WARNING: Make Sure Pin("<<newNo<<") is Configured for GPIO in raspi_config.......\n";
		}

		newNo = Str_ExtractNumber(newNo);					// To Remove any Prefix if Exists, Text Formating

		ValidPinNo = isValid_PinNo(newNo);					// *******Test Purpose******* To test The  Functioning of Str_ExtractNumber function
		if (ValidPinNo == -1)				return (int)255;		//            Sole Testing Purpose            //

		unexport_Success = unexport_PinNo(PinNo);				// First unexport existing PinNo
		if (unexport_Success != 0) {						//	unexport_Success == 0 => Successfull Unexport
			printf("Unexport of existing Pin Failed........ Pin not set \n");
			return unexport_Success;
		}

		printf("Unexported existing Pin ........ Exporting new Pin...... \n");
		PinNo = "0";															// "0" Value in never written on the system file

		export_Success = export_PinNo(newNo);
		if (export_Success != 0) {										//	export_Success == 0 => Successfull Export;
			printf("Export of new Pin Failed........ Pin not set \n");
			printf("WARNING: Currently NO Pin Is Alloted to this Object \n");
			return unexport_Success;
		}

			PinNo = newNo;
			cout << "Export of new Pin("<<PinNo<<") Successful !!  \n";

			return 0;
		
	}				
	unsigned int get_PinNo() {

		return (unsigned int)atoi(PinNo.c_str());					// atoi() converts C string to integer 
	}

	int set_PinDir(string Dir) {
		int export_Succcess;
		int ValidDir;
																								 
		ValidDir = isValid_PinDir(Dir);
		if (ValidDir == -1) {
			printf("Invalid PinMode \n");			// Validity Check

			return (int)255;
		}

		if (ValidDir == (int)Input)			Dir = "in";								// Formating Text
		if (ValidDir == (int)Output)		Dir = "out";							

		export_Succcess = export_PinDir(Dir, PinNo);
		if (export_Succcess != 0) {
			printf("Failed To set Pin Mode...... \n");
			return export_Succcess;
		}
		
		PinDir = Dir;
		cout<<"Successfully set Pin Mode to "<< PinDir << "!!  \n";

		return 0;
	}				
	enum PinDir get_PinDir() {

		if (PinDir == "in")			return Input;
		if (PinDir == "out")		return Output;
		else						return PinDir::Undefined;
	}

	int set_PinVal(string Val) {
		int export_Succcess;
		int ValidPinVal;
			
		if (PinDir != "out")					return 254;				// set_PinVal only possible if PinMode is Output

		ValidPinVal = isValid_PinVal(Val);
		while ( ValidPinVal == -1) {
			printf("Invalid Pin Output \n");				// Validity Check

			return (int)255;
		}

		if (ValidPinVal == (int)Low)				Val = "0";								// Formating Text
		if (ValidPinVal == (int)High)				Val = "1";

		export_Succcess = export_PinVal(Val, PinNo);
		if (export_Succcess != 0) {
			printf("Failed To set Value...... \n");
			return export_Succcess;
		}

		PinVal = Val;
		cout<<"Successfully set Value("<<PinVal<<")!!  \n";

		return 0;

	}		
	enum PinVal get_PinVal() {

		if (PinVal == "1")		return High;
		if (PinVal == "0")		return Low;
		else					return PinVal::Undefined;
	}
	int read_PinVal() {
		string import_Val;
		
		
		if (PinDir != "in") {									// Import PinVal is Only possible if PinMode is Input

			printf("Unable to Read Input ...... Invalid Pin Mode \n");
			return 254;
		}
			
		import_Val = import_PinVal(PinNo);									

		if (import_Val == "-1") {
		
			printf("Unable to Read Input ........ Did not Update \n");
			return -1;
		}

		PinVal = import_Val;											// Setting Value
		return (int)get_PinVal();
	}

private:																// 	(Risky Methods) Not to be used directly hence Private, (may manipulate undesired pin)
	GPIOClass(string No, string Dir, string Val) {
		export_PinNo(No);			PinNo = No;
		export_PinDir(Dir, No);		PinDir = Dir;
		export_PinVal(Val, No);		PinVal = Val;
	}

	int export_PinNo(string No) {
		ofstream exportGPIO("/sys/class/gpio/export");										// Open "export" file. 
		if (!exportGPIO.is_open()) {
			cout << " OPERATION FAILED: Unable to export GPIO" << No << endl;
			return -1;
		}

		exportGPIO << No;						//write GPIO number to export
		exportGPIO.close();						//close export file
		return 0;
	}
	int unexport_PinNo(string No) {															//Unexport Existing PinNo
		ofstream unexportGPIO("/sys/class/gpio/unexport");										// Open "unexport" file. 
		if (!unexportGPIO.is_open()) {
			cout << " OPERATION FAILED: Unable to Open unexport file" << No << endl;
			return -1;
		}

		unexportGPIO << No;							//write GPIO number to unexport
		unexportGPIO.close();						//close unexport file
		return 0;
	}

	int export_PinDir(string Dir, string No) {
		string systemAddress_str = "/sys/class/gpio/gpio" + No + "/direction";
		ofstream exportDirGPIO(systemAddress_str.c_str());											// open direction file for gpio
		if (!exportDirGPIO.is_open()) {
			cout << " OPERATION FAILED: Unable to Open direction file of GPIO" << No << endl;
			return -1;
		}

		exportDirGPIO << Dir;															//write direction to direction file
		exportDirGPIO.close();															// close direction file
		return 0;
	}

	int export_PinVal(string Val, string No) {
		
		string systemAddress_str = "/sys/class/gpio/gpio" + No + "/value";
		ofstream exportValGPIO(systemAddress_str.c_str());											// open value file for gpio
		if (!exportValGPIO.is_open()) {
			cout << " OPERATION FAILED: Unable to Open value file of GPIO" << No << endl;
			return -1;
		}

		exportValGPIO << Val;															//write value to value file
		exportValGPIO.close();															// close value file

		return 0;
	}
	string import_PinVal(string No) {
		string Val;

		string systemAddress_str = "/sys/class/gpio/gpio" + No + "/value";
		ifstream importValGPIO(systemAddress_str.c_str());											// open value file for gpio
		if (!importValGPIO.is_open()) {
			cout << " OPERATION FAILED: Unable to Open value file of GPIO" << No << endl;
			return "-1";
		}

		importValGPIO >> Val ;															//read value 
		importValGPIO.close();															// close value file

		return Val;
	}

};

class Switch {

	unsigned int ID;
	enum Type Type;
	float PowerRating;
	enum Status Status;
	GPIOClass GPIO;
	string StatFileAddrs;

	enum Event{Loaded, Verifying_Config, Repairing_Config, Initialised, Initialisation_Failed, Status_Initialised, Status_Unchanged, Status_Changed_Locally, Status_Changed_Remotely, Calculated_Power, Unloaded};
	struct DataEntry {
		time_t EventTime;
		enum Event;
		string Value;
	};

public:
	bool is_Enabled;

public:
	Switch() {}
	Switch(unsigned int IDNo, enum Type enumType, float fPowerRating, enum Status enumStatus, GPIOClass SwitchGPIO, string DatafileAddress = "", bool blnwas_Enabled = false) {
		ID = IDNo;
		Type = enumType;
		PowerRating = fPowerRating;
		Status = enumStatus;
		GPIO = SwitchGPIO;
		StatFileAddrs = DatafileAddress;
		is_Enabled = blnwas_Enabled;
	}
	~Switch() {
		(*this).WriteOn_Datafile("Unloaded\n");
	}

	void Initialise_Manual(unsigned int IDNo) {

		string type;
		string status;
		string is_enabled;

		ID = IDNo;

		printf("  Configure the Switch(ID:%u): \n", ID);

		printf("                  Switch Type: ");
		cin >> type;
		while (isValid_type(type) == -1) {
			printf("Invalid Type entered......Please reenter : ");
			cin >> type;
		}

		Type = StringToEnum_Type(type);

		printf("         Power Rating (in kW): ");
		scanf_s("%f", &PowerRating);

		printf("       Initial Status(On/Off): ");
		cin >> status;
		while (isValid_status(status) == -1) {
			printf("Invalid Switch Status entered......Please reenter : ");
			cin >> status;
		}

		Status = StringToEnum_Status(status);

		GPIO.Initialise_Manual();									// Initialisation of GPIO Class

		(*this).Ini_Datafile("Data\SwitchID#" + to_string(ID) + ".dat");	 // Initialise Stat File

		printf("    Enable Switch(true/false): ");
		cin >> is_enabled;

		while (isValid_is_enabled(is_enabled) == -1) {
			printf("Invalid Input ............ Please reenter : ");
			cin >> is_enabled;
		}

		is_Enabled = StringToBool_is_Enabled(is_enabled);
	}
	int Initialise_Auto(ifstream &ConfigFile, string &TempBuff, string &TempVal) {
		const int SwitchAttribNo = 6;
		unsigned char SwitchFlags;

		for (int j = 0; j < SwitchAttribNo; j++) {
			getline(ConfigFile, TempBuff, '=');
			ConfigFile.seekg(1, ios::_Seekcur);
			getline(ConfigFile, TempVal);
																							
			if (TempBuff == "IDNo")				ID = atoi(TempVal.c_str());
			if (TempBuff == "enumType")			Type = StringToEnum_Type(TempVal);
			if (TempBuff == "fPowerRating")		PowerRating = atof(TempVal.c_str());
			if (TempBuff == "enumStatus")		Status = StringToEnum_Status(TempVal);
			if (TempBuff == "DatafileAddress")	StatFileAddrs = Str_deQuote(TempVal);
			if (TempBuff == "[Switch.GPIO]")    GPIO.Initialise_Auto(ConfigFile, TempBuff, TempVal);
			else								break;
		}	
		(*this).WriteOn_Datafile("Loaded", "Data\SwitchID#_temp_.dat");

		is_Enabled = (*this).isValid_Config(SwitchFlags);
		(*this).WriteOn_Datafile("Verifing Config..........("+ to_string(SwitchFlags)+ ")", "Data\SwitchID#_temp_.dat");
		
		if (is_Enabled != true) {
			(*this).Repair_Config(SwitchFlags);
			is_Enabled = (*this).isValid_Config(SwitchFlags);
			(*this).WriteOn_Datafile("Repairing Config..........(" + to_string(SwitchFlags) + ")", "Data\SwitchID#_temp_.dat");
		}

		if (is_Enabled == true) {															
			ifstream DataTemp("Data\SwitchID#_temp_.dat");
		
			if (fileExists(StatFileAddrs) == false)				(*this).Ini_Datafile();						// Initialising Datafile if First time

			ofstream Datafile(StatFileAddrs, ios::app);					// Make the Datafile Permanent

			Movefile(DataTemp, Datafile);

			DataTemp.close();
			remove("Data\SwitchID#_temp_.dat");
			Datafile.close();

			(*this).WriteOn_Datafile("Initialised");
		}
		else		(*this).WriteOn_Datafile("Initialisation Failed");

		return SwitchFlags;											//***WARNING*** implicit conversion from unsigned char to signed int
	}	

	int set_Status(enum Status enumStatus, bool LocalMode) {
		int set_PinVal_Success;

		if (enumStatus == Status::Off)					set_PinVal_Success = GPIO.set_PinVal("0");
		if (enumStatus == Status::On)					set_PinVal_Success = GPIO.set_PinVal("1");
		if (enumStatus == Status::Undefined)			return -1;

		if (set_PinVal_Success == -1)				return -1;

		Status = enumStatus;
		if (LocalMode == true)			(*this).WriteOn_Datafile("Changed Status Locally..........(" + EnumToString_status(Status) + ")");
		else							(*this).WriteOn_Datafile("Changed Status Remotely..........(" + EnumToString_status(Status) + ")");

		return 0;
	}

	float calc_PowerConsumption(string from , string to ) {								// Complete the function
		ifstream Datafile(StatFileAddrs.c_str());  string TempBuff, TempVal;
		string time, 


			do  getline(Datafile, TempBuff); 					// Skip to Data Section
	    while (TempBuff.find("[.Data]") != string::npos);


	}

	int RawRead_Datafile(vector<string[3]> &RawData) {										// Try to make RawData a Static Variable
		ifstream Datafile(StatFileAddrs.c_str()); 
		string TempBuff;
		
		Datafile.seekg(160, ios_base::beg);								// Skip to Data Section

		getline(Datafile, TempBuff);
		if (TempBuff.find("[.Data]") != string::npos) {					// Unnecessary if Condition
			do {
				string time = "\0", entry = "\0", val = "\0";
			
				getline(Datafile, TempBuff, ' ');
				Datafile.seekg(1, ios_base::cur);
				time = TempBuff;

				getline(Datafile, TempBuff);		// delimitor '\n'
				entry = TempBuff.substr(0, TempBuff.length() - TempBuff.find(".........."));
				val = TempBuff.substr(TempBuff.find("(") + 1, TempBuff.find(")") - (TempBuff.find("(") + 1));

				RawData.push_back({ time, entry, val });
			} while (Datafile.eof() == false);
		}

		Datafile.close();
		return 0;
	}

	// ToDo: Make DataEntry Categorise function, Make Higherlevel Read_Datafile function
	int Categorise_DataEntry(const vector<string[3]> RawData, vector<unsigned char> &Data_flags) {

	}


	int WriteCurr_Config(ofstream &ConfigFile, bool ConfigLong) {			// Long Config Used for ConfigFile, Short Config used for Datafile initialisation
		const char dQuote = (char)34;								//dQuote => Double Quote " 

		ConfigFile << "[Switch]\n";
		ConfigFile << "IDNo=" << ID << "\n";
		ConfigFile << "enumType=" << EnumToString_type(Type) << "\n";
		ConfigFile << "fPowerRating=" << PowerRating << "\n";

		if(ConfigLong == true){		
			ConfigFile << "enumStatus=" << EnumToString_status(Status) << "\n";
			GPIO.WriteCurr_Config(ConfigFile, true);
			ConfigFile << "[Switch.Data]\n";
			ConfigFile << "DatafileAddress=" << dQuote << StatFileAddrs << dQuote << "\n";
			ConfigFile << "blnwas_Enabled=" << BoolToString_is_enabled(is_Enabled) << "\n";
		}
		else	GPIO.WriteCurr_Config(ConfigFile, false);

		return 0;
	}
	bool isValid_Config(unsigned char &Setflags) {							// Validity check for auto initialisation
		const unsigned char _NOT_VALID_ID = 0x80;
		const unsigned char _NOT_VALID_TYPE = 0x40;
		const unsigned char _NOT_VALID_POWERRATING = 0x20;
		const unsigned char _NOT_VALID_STATUS = 0x10;
		unsigned char GPIO_FLAGS; 
		const unsigned char _NOT_VALID_DATAFILEADDRS = 0x01;

		if(ID == 0)											Setflags = Setflags | _NOT_VALID_ID;
		if(Type == Type::Undefined)							Setflags = Setflags | _NOT_VALID_TYPE;
		if(PowerRating == 0.0f)								Setflags = Setflags | _NOT_VALID_POWERRATING;
		if(Status == Status::Undefined)						Setflags = Setflags | _NOT_VALID_STATUS;
		
		GPIO.isValid_Config(GPIO_FLAGS);
		GPIO_FLAGS * 2;	/*Right shift */				    Setflags = Setflags | GPIO_FLAGS;

		ofstream Datafile(StatFileAddrs);
		if (Datafile.good() == false)						Setflags = Setflags | _NOT_VALID_DATAFILEADDRS;

		if (Setflags == 0)									return true;
		else												return false;
	}	
	int Repair_Config(unsigned char SwitchFlags) {						// Must improve !!
		const unsigned char _NOT_VALID_ID = 0x80;
		const unsigned char _NOT_VALID_TYPE = 0x40;
		const unsigned char _NOT_VALID_POWERRATING = 0x20;
		const unsigned char _NOT_VALID_STATUS = 0x10;
		const unsigned char _NOT_VALID_GPIO_PINNO = 0x04 * 2;			//  GPIOFlags to SwitchFlags Conversion
		const unsigned char _NOT_VALID_GPIO_PINDIR = 0x02 * 2;
		const unsigned char _NOT_VALID_GPIO_PINVAL = 0x01 * 2;
		const unsigned char _NOT_VALID_DATAFILEADDRS = 0x01;

		unsigned char UpdatedFlags = SwitchFlags;
		
		if (SwitchFlags & _NOT_VALID_TYPE) {
			Type = Other;
			UpdatedFlags = UpdatedFlags - _NOT_VALID_TYPE;
		}

//		if (SwitchFlags & _NOT_VALID_ID)				return UpdatedFlags;

		if (SwitchFlags & _NOT_VALID_DATAFILEADDRS) {
			StatFileAddrs = "Data\SwitchID#" + to_string(ID) + ".dat";
			UpdatedFlags = UpdatedFlags - _NOT_VALID_DATAFILEADDRS;
		}

		return UpdatedFlags;										//***WARNING*** implicit conversion from unsigned char to signed int
	}

private:
	int WriteOn_Datafile(string Write) {
		return WriteOn_Datafile(Write, StatFileAddrs);
	}
	int WriteOn_Datafile(string Write, string FileAddrs) {
		ofstream Datafile(FileAddrs);
		if (!Datafile.good())			return -1;

		Datafile << currentDateTime() << " " << Write << "\n";
		Datafile.close();

		return 0;
	}
	int Ini_Datafile(string FileAddrs = "Data\SwitchID#_temp_.dat") {
		
		ofstream Datafile(FileAddrs);
		
		if (Datafile.good() != true)			return -1;

		Datafile << "[#3 Project Home Automation] Data Collection" << "\n";
		Datafile << "[Configuration]" << "\n";
		(*this).WriteCurr_Config(Datafile, false);						// false => short Config
		Datafile << "\n";
		Datafile << "[.Data]";
		Datafile << "\n";
		Datafile.close();

		return 0;
	}
};
	
int isValid_OpMode(string opMode) {

	const vector<string> Valid_Auto = { "auto", "a", "automatic" };
	const vector<string> Valid_Manual = { "manual", "m" };

	std::transform(opMode.begin(), opMode.end(), opMode.begin(), ::tolower);

	for (int i = 0; i < Valid_Auto.size(); i++)				if (opMode == Valid_Auto[i])					return (int)Auto;
	for (int i = 0; i < Valid_Manual.size(); i++)			if (opMode == Valid_Manual[i])					return (int)Manual;

	return -1;
}
enum Mode StringToEnum_OpMode(string opMode) {

	int ValidopMode = isValid_OpMode(opMode);

	if (ValidopMode != -1)				return (enum Mode)ValidopMode;
	else	printf("Error: StringToEnum_OpMode() couldn't convert to enum \n");
	
	return Mode::Undefined;
}
string EnumToString_opMode(enum Mode OpMode) {
	string opMode;
	
	if (OpMode = Auto)			opMode = "Auto";
	if (OpMode = Manual)		opMode = "Manual";

	return opMode;
}

int isValid_type(string type) {									// Type => enum Type; type => string type

	const vector<string> Valid_Light = {"light", "tubelight"};
	const vector<string> Valid_Fan = { "fan", "ceilingfan" };
	const vector<string> Valid_PowerSocket = { "powersocket", "socket", "plugpoint" };
	const vector<string> Valid_Other = { "other" };


	type = Str_deSpace(type);																		// Removes Space Char
	std::transform(type.begin(), type.end(), type.begin(), ::tolower);									// Transform all to lowercase 

	for (int i = 0; i < Valid_Light.size(); i++)				if (type == Valid_Light[i])			return (int)Light; 
	for (int i = 0; i < Valid_Fan.size(); i++)					if (type == Valid_Fan[i])			return (int)Fan;
	for (int i = 0; i < Valid_PowerSocket.size(); i++)			if (type == Valid_PowerSocket[i])	return (int)PowerSocket ;
	for (int i = 0; i < Valid_Other.size(); i++)				if (type == Valid_Other[i])			return (int)Other;

	return -1;
}
enum Type StringToEnum_Type(string type) {

	int Validtype = isValid_type(type);

	if (Validtype != -1)	return (enum Type)Validtype;
	else	printf("Error: StringToEnum_Type() couldn't convert to enum \n");

	return Type::Undefined;
}				
string EnumToString_type(enum Type Type) {
	string type;
	if (Type = Light)			type = "Light";
	if (Type = Fan)				type = "Fan";
	if (Type = PowerSocket)		type = "Power Socket";
	if (Type = Other)			type = "Other";

	return type;
}
																	
int isValid_status(string status) {							// Status => enum Status; status => string status

	const vector<string> Valid_On = { "on", "high", "1" };
	const vector<string> Valid_Off = { "off", "low", "0" };

	std::transform(status.begin(), status.end(), status.begin(), ::tolower);							// Transform all to lowercase 

	for (int i = 0; i < Valid_On.size(); i++)					if (status == Valid_On[i])			return (int)On;
	for (int i = 0; i < Valid_Off.size(); i++)					if (status == Valid_Off[i])			return (int)Off;

	return -1;
}
enum Status StringToEnum_Status(string status) {

	int Validstatus = isValid_status(status);

	if (Validstatus != -1)		return (enum Status)Validstatus;
	else	printf("Error: StringToEnum_Status() couldn't convert to enum \n");

	return Status::Undefined;
}
string EnumToString_status(enum Status Status) {
	string status;
	if (Status = On)			status = "On";
	if (Status = Off)			status = "Off";

	return status;
}

int isValid_is_enabled(string is_enabled) {

	const vector<string> Valid_Enable = { "true", "yes", "y", "enable", "on", "1" };
	const vector<string> Valid_Disable = { "false", "no", "n", "disable", "off", "0" };

	std::transform(is_enabled.begin(), is_enabled.end(), is_enabled.begin(), ::tolower);

	for (int i = 0; i < Valid_Enable.size(); i++)				if (is_enabled == Valid_Enable[i])			return (int)true;
	for (int i = 0; i < Valid_Disable.size(); i++)				if (is_enabled == Valid_Disable[i])			return (int)false;

	return -1;
}
bool StringToBool_is_Enabled(string is_enabled) {

	int Validis_enabled = isValid_is_enabled(is_enabled);

	if (Validis_enabled != -1)			return (bool)Validis_enabled;
	else	printf("Error: StringToBool_is_Enabled() couldn't convert to bool \n");
}
string BoolToString_is_enabled(bool is_Enabled) {
	string is_enabled;

	if (is_Enabled == true)			is_enabled = "true";
	if (is_Enabled == false)			is_enabled = "false";

	return is_enabled;
}
																// String Processing Tools for unformatted User Input
string Str_deSpace(string s) {				// Removes any Space Char in the Given String
	unsigned int findPos;

	findPos = s.find(" ");
	while (findPos != s.npos)	
	{
		findPos = s.find(" ");					// Find function returns position of the first occurance of Arguement in the string, if not found returns string::s.npos
		s.erase(findPos, 1);						// Erase removes the part of the string s.erase(StartPoint,  LengthToRemove);
	}

	return s;
}
string Str_ExtractNumber(string s) {
	bool NumFound;
	unsigned int NumStrt, Numlen = 0;
	
	for (int i = 0; i < s.length(); i++) {
		NumFound = (s[i] > '0') && (s[i] < ':');							//	initially Exclude '0' => No Leading 0's	(True for [1-9])

		if (NumFound) {
			NumStrt = i;
			Numlen = 1;

			while(NumFound) {
				NumFound = (s[i+1] > '/') && (s[i+1] < ':');					// Including all 0's after atleast one Non 0 (True for [0-9])
				Numlen++;

				i++;
			}
			break;															// Finds Length of first consequtive list of numbers ONLY 
		}
	}

	if (Numlen = 0)			return (string)"";
	else					return s.substr(NumStrt, Numlen);										// Returns only the Firt list of Numbers 
}
string Str_deQuote(string s) {					// Removes Quotes that enclose the string if they exist
	unsigned int findPos;
	const char dQuote = (char)34;				// Char 34 = "	        doubleQuotes

	findPos = s.find_first_of(dQuote);		
	s.erase(findPos, 1);

	findPos = s.find_last_of(dQuote);
	s.erase(findPos, 1);

	return s;
}

int isValid_PinNo(string No){
	const vector<string> Valid_Prefix = { "gpio", "pin", "gpio0", "pin0" };
	const vector<string> Valid_GPIOPins = { "4", "17", "27", "22", "9", "5", "6", "13", "19", "26", "18", "23", "24", "25", "8", "7", "12", "16", "20", "21" };
	const vector<string> Maybe_GPIOPins = { "2", "3", "14", "15" };
	const vector<string> Not_GPIOPins = { "10", "11" };

	No = Str_deSpace(No);																		// Removes Space Char
	std::transform(No.begin(), No.end(), No.begin(), ::tolower);									// Transform all to lowercase 

	for (int i = 0; i < Valid_Prefix.size(); i++) {											// Finds if a Prefix Exists and removes it
		unsigned int findPos = No.find(Valid_Prefix[i]);

		if (findPos != No.npos)				No.erase(findPos, Valid_Prefix[i].length());
	}

	for (int i = 0; i < Valid_GPIOPins.size(); i++)					if (No == Valid_GPIOPins[i])			return 0;
	for (int i = 0; i < Maybe_GPIOPins.size(); i++)					if (No == Maybe_GPIOPins[i])			return 1;
	for (int i = 0; i < Not_GPIOPins.size(); i++)					if (No == Not_GPIOPins[i])			    return -1;

	return -1;
}
int isValid_PinDir(string Dir){
	const vector<string> Valid_PinDir_In = { "in", "input", "read", "digitalread" };
	const vector<string> Valid_PinDir_Out = { "out", "output", "write", "digitalwrite" };

	Dir = Str_deSpace(Dir);														// Removes Space Char
	std::transform(Dir.begin(), Dir.end(), Dir.begin(), ::tolower);					// Transform all to lowercase 

	for (int i = 0; i < Valid_PinDir_In.size(); i++)				 if (Dir == Valid_PinDir_In[i])				return (int)Input;
	for (int i = 0; i < Valid_PinDir_Out.size(); i++)				 if (Dir == Valid_PinDir_Out[i])			return (int)Output;

	return PinDir::Undefined;
}
int isValid_PinVal(string Val){
	const vector<string> Valid_PinVal_0 = { "0", "low", "off", "+0v" };
	const vector<string> Valid_PinVal_1 = { "1", "high", "on", "+5v" };

	std::transform(Val.begin(), Val.end(), Val.begin(), ::tolower);					// Transform all to lowercase 

	for (int i = 0; i < Valid_PinVal_0.size(); i++)						if (Val == Valid_PinVal_0[i])		return (int)Low;
	for (int i = 0; i < Valid_PinVal_1.size(); i++)						if (Val == Valid_PinVal_1[i])		return (int)High;

	return PinVal::Undefined;
}
																// Functional Tools
const string currentDateTime() {			// Get current date/time, format is [YYYY-MM-DD.HH:mm:ss]
	time_t     now = time(0);
	struct tm  tstruct;
	localtime_s(&tstruct, &now);

	string	   Format_time = TmToString_time(tstruct);
	return Format_time;
}
string TmToString_time(struct tm Time) {	
	char       buf[20];
	const char *format = "%Y-%m-%d.%X";
	string	   Format_time;

	strftime(buf, sizeof(buf), format, &Time);

	Format_time = "[" + string(buf) + "]";

	return Format_time;
}
struct tm StringToTm_Time(string time){

	struct tm Time;
	const char *format = "%Y-%m-%d.%X";
	string Deformat_time = time.substr((size_t)1, time.size()-2);

	strptime(Deformat_time.c_str(), format, &Time);

	return Time;

}
time_t StringToTime_t_Timet(string time) {

	struct tm Time;
	Time = StringToTm_Time(time);

	time_t Timet = mktime(&Time);
	return Timet;
}
char *strptime(const char *str, const char *fmt, struct tm *tm) {
	char *fmt_ptr; char fmt_char = '\0';
	char buf[10];
	fmt_ptr = fmt;

	while (*fmt_ptr != '\n') {
		if (*fmt_ptr == '%') { fmt_ptr++;  fmt_char = *fmt_ptr; fmt_ptr++;}
		
		switch (fmt_char)
		{
		case 'Y':
			strcpy_s(buf, 4, fmt_ptr);
			tm->tm_year = atoi(buf) - 1900;
			fmt_ptr += 4;
			break;

		case 'm':
			strcpy_s(buf, 2, fmt_ptr);
			tm->tm_mon = atoi(buf) - 1;
			fmt_ptr += 2;
			break;

		case 'd':
			strcpy_s(buf, 2, fmt_ptr);
			tm->tm_mday = atoi(buf);
			fmt_ptr += 2;
			break;

		case 'H':
			strcpy_s(buf, 2, fmt_ptr);
			tm->tm_hour = atoi(buf);
			fmt_ptr += 2;
			break;

		case 'M':
			strcpy_s(buf, 2, fmt_ptr);
			tm->tm_min = atoi(buf) - 1;
			fmt_ptr += 2;
			break;

		case 'S':
			strcpy_s(buf, 2, fmt_ptr);
			tm->tm_sec = atoi(buf);
			fmt_ptr += 2;
			break;

		case 'X':
			strcpy_s(buf, 2, fmt_ptr);
			tm->tm_hour = atoi(buf);
			fmt_ptr += 2;

			strcpy_s(buf, 2, fmt_ptr);
			tm->tm_min = atoi(buf) - 1;
			fmt_ptr += 2;

			strcpy_s(buf, 2, fmt_ptr);
			tm->tm_sec = atoi(buf);
			fmt_ptr += 2;
			break;
		}
			
		fmt_ptr++;
	}

	return fmt_ptr;
}

int Movefile(ifstream &S, ofstream &D) {
	string Buff;

	while (!S.eof()) {
		getline(S, Buff);
		D << Buff << endl;
	}

	return 0;
}
bool fileExists(const string FileName){
	ifstream infile(FileName);
	return infile.good();
}
int ID_Gen(){}

void Default_Config() {
	int WriteDefault_Config_Success;

	arrSwitch.erase(arrSwitch.begin(), arrSwitch.end());								// Erase Full Array
	OperationMode = Mode::Auto;

	WriteDefault_Config_Success = WriteDefault_Config();
	if (WriteDefault_Config_Success == -1) {
		cout << "Unable to write Default Config to file.......\n";
	}
}
int WriteDefault_Config() {
	ofstream ConfigFile("config.cfg", fstream::out | fstream::trunc);			// truncate mode => Delete existing file if it exists and Create new

	if (!ConfigFile.is_open())			return -1;

	ConfigFile << "[Global Settings]\n";
	ConfigFile << "TotalSwitchNumber=0\n"; 
	ConfigFile << "enumOpMode=Auto\n";
	ConfigFile << "\n";

	return 0;
}

bool isValid_GlobalConfig(unsigned char &Setflags){
	const unsigned char _NOT_VALID_OPERATIONMODE = 0x01;

	Setflags = 0;

	if (OperationMode == Mode::Undefined)		Setflags = Setflags | _NOT_VALID_OPERATIONMODE;

	if(Setflags == 0)					return true;
	else								return false;
}
int Repair_GlobalConfig(unsigned char Globalflags){
	const unsigned char _NOT_VALID_OPERATIONMODE = 0x01;

	unsigned char UpdatedFlags = Globalflags;

	if (Globalflags & _NOT_VALID_OPERATIONMODE) {
		OperationMode = Mode::Auto;
		UpdatedFlags = UpdatedFlags - _NOT_VALID_OPERATIONMODE;
	}

	return UpdatedFlags;										//***WARNING*** implicit conversion from unsigned char to signed int
}

void Manual_Config() {
	Switch SwitchTemp;
	string opMode;
	char charYesNo;

		printf("Manual Configure Switches: \n");
		printf("\n");

	do {
		SwitchTemp.Initialise_Manual(ID_Gen());
		arrSwitch.push_back(SwitchTemp);
		printf("\n");

		printf("Configure Another Switch(y/n): ");
		scanf_s("%c", &charYesNo);
	} while (charYesNo == 'y' || charYesNo == 'Y');
	printf("\n");

	printf("Operation Mode(auto/manual): ");
	cin >> opMode;
	while (isValid_OpMode(opMode) == -1) {
		printf("Invalid Operation Mode entered......Please reenter : ");
		cin >> opMode;
	}
}
int Load_Config() {											
	string TempBuff;
	string TempVal;
	ifstream ConfigFile("config.cfg");

	const int GlobalAttribNo = 2;
	unsigned char GlobalFlags;

	int TotalSwitchNumber;

	vector<int> SwitchConfigFlags;
																								////// ******** ERROR MANAGEMENT REQUIRED IMPORTANT ******** //////
	if (!ConfigFile.is_open())			return -1;

	getline(ConfigFile, TempBuff);

	if (TempBuff == "[Global Settings]") {

		for (int j = 0; j < GlobalAttribNo; j++) {
			getline(ConfigFile, TempBuff, '=');
			ConfigFile.seekg(1, ios::_Seekcur);
			getline(ConfigFile, TempVal);

			if (TempBuff == "TotalSwitchNumber")			TotalSwitchNumber = atoi(TempVal.c_str());
			if (TempBuff == "enumOpMode")					OperationMode = StringToEnum_OpMode(TempVal);
			else											break;
		}
	}
	if (isValid_GlobalConfig(GlobalFlags) == false)			GlobalFlags = Repair_GlobalConfig(GlobalFlags);				//Repairing Global Config
		
	if (TotalSwitchNumber == 0) {
		Manual_Config();
		return 0;
	}

	arrSwitch.resize(TotalSwitchNumber);										// Mem_Alloc For Switches
	SwitchConfigFlags.resize(TotalSwitchNumber);

		while(TempBuff == "" || !ConfigFile.eof())		getline(ConfigFile, TempBuff);				// Used to ignore Empty Lines

		/*********************************** Imperfect Code*********************************************/
		for (int i = 0; i < TotalSwitchNumber; i++) {
			if (TempBuff == "[Switch]") {
				SwitchConfigFlags[i] = arrSwitch[i].Initialise_Auto(ConfigFile, TempBuff, TempVal);
				while (TempBuff == "" || !ConfigFile.eof())		getline(ConfigFile, TempBuff);				// Used to ignore Empty Lines
			}																						//Do Something more with flags
		}




}
void Save_Config() {
	ofstream ConfigFile("config.cfg", fstream::out | fstream::trunc);			// truncate mode => Delete existing file if it exists and Create new

	ConfigFile << "[Global Settings]\n";
	ConfigFile << "TotalSwitchNumber=" << arrSwitch.size() << "\n";
	ConfigFile << "enumOpMode=" << EnumToString_opMode(OperationMode) << "\n";
	ConfigFile << "\n";

	for (int i = 0; i < arrSwitch.size(); i++) {
		arrSwitch[i].WriteCurr_Config(ConfigFile, true);						// true => Long Config
		ConfigFile << "\n";
	}
	ConfigFile.close();
}

int main(int argc, char *argv[])
{													// Initiallisation


	
	if (fileExists("config.cfg") == true)						Load_Config();
	else														Default_Config();

/*	while (1) {
		for (int i = 0; i < arrSwitch.size(); i++) {
			if (arrSwitch[i].is_Enabled == true) {

			}
		}
	}							*/		







    return 0;
}

