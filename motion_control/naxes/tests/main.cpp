//hardware interfaces
#include "hardware/kuka/Kuka160nAxesVelocityController.hpp"
#include "hardware/kuka/Kuka361nAxesVelocityController.hpp"

//User interface
#include "taskbrowser/TaskBrowser.hpp"

//Reporting
#include "reporting/FileReporting.hpp"

//nAxes components
#include "motion_control/naxes/nAxesComponents.hpp"

//#include <rtt/Activities.hpp>
#include <rtt/GenericTaskContext.hpp>
//#include <rtt/Logger.hpp>
#include <rtt/os/main.h>

using namespace Orocos;
using namespace RTT;
using namespace std;

class EmergencyStop
{
public:
  EmergencyStop(GenericTaskContext *axes)
    : _axes(axes) {
    _stop = _axes->commands()->getCommand<bool(int,double)>("stopAxis");
    _lock = _axes->commands()->getCommand<bool(int,double)>("lockAxis");
  };
  ~EmergencyStop(){};
  void callback(int axis, double value) {
    _axis = axis;
    _value = value;
    _stop(axis,value);
    _lock(axis,value);
    cout << "---------------------------------------------" << endl;
    cout << "--------- EMERGENCY STOP --------------------" << endl;
    cout << "---------------------------------------------" << endl;
    cout << "Axis "<< _axis <<" drive value "<<_value<< " reached limitDriveValue"<<endl;
  };
private:
  GenericTaskContext *_axes;
  Command<bool(int,double)> _stop;
  Command<bool(int,double)> _lock;
  int _axis;
  double _value;
}; // class

void PositionLimitCallBack(int axis, double value)
{
  Logger::log()<<Logger::Warning<< "-------------Warning----------------"<<Logger::endl;
  Logger::log()<<Logger::Warning<< "Axis "<<axis<<" moving passed software position limit, current value: "<<value<<Logger::endl;
}

// main() function

int ORO_main(int argc, char* argv[])
{
  GenericTaskContext* my_robot = NULL;
  if (argc > 1)
    {
      string s = argv[1];
      if(s == "Kuka361"){
	Logger::log()<<Logger::Warning<<"Choosing Kuka361"<<Logger::endl;
	my_robot = new Kuka361nAxesVelocityController("Robot");
      }
      else if(s == "Kuka160"){
	Logger::log()<<Logger::Warning<<"Choosing Kuka160"<<Logger::endl;
	my_robot = new Kuka160nAxesVelocityController("Robot");
      }
    }
  else{
    Logger::log()<<Logger::Warning<<"Using Default Kuka160"<<Logger::endl;
    my_robot = new Kuka160nAxesVelocityController("Robot");
  }

  if ( Logger::log().getLogLevel() < Logger::Info ) {
    Logger::log().setLogLevel( Logger::Info );
              Logger::log() << Logger::Info << argv[0] << " manually raises LogLevel to 'Info' (5). See also file 'orocos.log'."<<Logger::endl;
  }
  
  EmergencyStop _emergency(my_robot);
  
  // Creating Event Handlers
  Handle _emergencyHandle = my_robot->events()->setupConnection("driveOutOfRange").
    callback(&_emergency,&EmergencyStop::callback).handle();
  Handle _positionWarning = my_robot->events()->setupConnection("positionOutOfRange").
    callback(&PositionLimitCallBack).handle();

  // Connecting Event Handlers
  _emergencyHandle.connect();
  _positionWarning.connect();
  
  //nAxesComponents
  nAxesSensor sensor("nAxesSensor",6);
  nAxesGeneratorPos generatorPos("nAxesGeneratorPos",6);
  nAxesGeneratorVel generatorVel("nAxesGeneratorVel",6);
  nAxesControllerPos controllerPos("nAxesControllerPos",6);
  nAxesControllerPosVel controllerPosVel("nAxesControllerPosVel",6);
  nAxesControllerVel controllerVel("nAxesControllerVel",6);
  nAxesEffectorVel effector("nAxesEffectorVel",6);

  //connecting sensor and effector to hardware
  my_robot->connectPeers(&sensor);
  my_robot->connectPeers(&effector);
  
  //connection naxes components to each other
  generatorPos.connectPeers(&sensor);
  generatorVel.connectPeers(&sensor);
  controllerPos.connectPeers(&sensor);
  controllerPosVel.connectPeers(&sensor);
  controllerVel.connectPeers(&sensor);
  controllerPos.connectPeers(&generatorPos);
  controllerPosVel.connectPeers(&generatorPos);
  controllerVel.connectPeers(&generatorVel);
  effector.connectPeers(&controllerPos);
  effector.connectPeers(&controllerPosVel);
  effector.connectPeers(&controllerVel);
    
  //Reporting
  FileReporting reporter("Reporting");
  reporter.connectPeers(&sensor);
  reporter.connectPeers(&generatorPos);
  reporter.connectPeers(&generatorVel);
  reporter.connectPeers(&controllerPos);
  reporter.connectPeers(&controllerPosVel);
  reporter.connectPeers(&controllerVel);
  reporter.connectPeers(&effector);  

  //Create supervising TaskContext
  GenericTaskContext super("nAxes");
  
  // Link components to supervisor
  super.connectPeers(my_robot);
  super.connectPeers(&reporter);
  super.connectPeers(&sensor);    
  super.connectPeers(&generatorPos); 
  super.connectPeers(&generatorVel); 
  super.connectPeers(&controllerPos);
  super.connectPeers(&controllerPosVel);
  super.connectPeers(&controllerVel);
  super.connectPeers(&effector);

  // Load programs in supervisor
  super.loadProgram("program_calibrate_offsets.ops");
  super.loadProgram("program_moveto.ops");
  
  // Load StateMachine in supervisor
  super.loadStateMachine("states.osd");

    // Creating Tasks
  NonPreemptibleActivity _kukaTask(0.01, my_robot->engine() ); 
  NonPreemptibleActivity _sensorTask(0.01, sensor.engine() ); 
  NonPreemptibleActivity _generatorPosTask(0.01, generatorPos.engine() ); 
  NonPreemptibleActivity _generatorVelTask(0.01, generatorVel.engine() ); 
  NonPreemptibleActivity _controllerPosTask(0.01, controllerPos.engine() ); 
  NonPreemptibleActivity _controllerPosVelTask(0.01, controllerPosVel.engine() ); 
  NonPreemptibleActivity _controllerVelTask(0.01, controllerVel.engine() ); 
  NonPreemptibleActivity _effectorTask(0.01, effector.engine() ); 
  PeriodicActivity reportingTask(2,0.1,reporter.engine());
  PeriodicActivity superTask(1,0.1,super.engine());

  TaskBrowser browser(&super);
  browser.setColorTheme( TaskBrowser::whitebg );
  
  superTask.start();
  _kukaTask.start();
  
  //Load Reporterconfiguration and start Reporter
  //reporter.load();
  //reportingTask.start();
  
  // Start the console reader.
  browser.loop();
  
  return 0;
}