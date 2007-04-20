#include <motion_control/naxes/ReferenceSensor.hpp>

namespace OCL
{
    using namespace std;
    using namespace RTT;
    
    ReferenceSensor::ReferenceSensor(string name,int _nrofaxes):
        TaskContext(name),
        nrofaxes(_nrofaxes),
        _getReference("getReference",&ReferenceSensor::getReference,this),
        reference(_nrofaxes)
    {
        this->methods()->addMethod( &_getReference,"gets the reference switch value from a reference port",
                                    "axis","the reference port corresponding to this axis");
        char buf[80];
        for (int i=0;i<_nrofaxes;++i) {
            sprintf(buf,"reference%d",i);
            reference[i] = new RTT::ReadDataPort<bool>(buf);
            ports()->addPort(reference[i]);
        }
    }
    
    bool ReferenceSensor::getReference(int axis)
    {
        if ((0>=axis)&&(axis<nrofaxes)) {
	  //log(Error) <<"reference"<<axis<<" connected: "<<reference[axis]->ready()<<endlog();
	  // log(Error) << "reference"<<axis<<" " << reference[axis]->Get() << endlog();
            return reference[axis]->Get();
        } else {
            log(Error) << "parameter axis out of range" << endlog();
            return false;
        }
    }
}

