#include <argos3/core/utility/rate.h>
#include <argos3/core/utility/logging/argos_log.h>
#include <argos3/core/control_interface/ci_controller.h>
#include <argos3/core/utility/plugins/factory.h>
#include <argos3/core/utility/plugins/factory_impl.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include "real_robot.h"

using namespace argos;

/****************************************/
/****************************************/

CRealRobot* CRealRobot::m_pcInstance = NULL;

/****************************************/
/****************************************/

CRealRobot::CRealRobot() :
   m_pcController(NULL) {
   /* Set instance */
   m_pcInstance = this;
}

/****************************************/
/****************************************/

void CRealRobot::Init(const std::string& str_conf_fname,
                      const std::string& str_controller_id,
                       Aseba::DBusInterface* ThymioInterface) {
   /* Parse the .argos file */
   m_tConfiguration.LoadFile(str_conf_fname);
   m_tConfRoot = *m_tConfiguration.FirstChildElement();
   /*
    * Get the control rate and timer
    */
   TConfigurationNode& tFramework   = GetNode(m_tConfRoot, "framework");
   TConfigurationNode& tExperiment  = GetNode(tFramework, "experiment");
   GetNodeAttribute(tExperiment, "ticks_per_second", m_fRate);

   GetNodeAttribute(tExperiment, "length", m_Time);

   /*
    * Parse XML to identify the controller to run
    */
   std::string strControllerId, strControllerTag;
   TConfigurationNode& tControllers = GetNode(m_tConfRoot, "controllers");
   TConfigurationNodeIterator itControllers;
   /* Search for the controller tag with the given id */
   for(itControllers = itControllers.begin(&tControllers);
       itControllers != itControllers.end() && strControllerTag == "";
       ++itControllers) {
      GetNodeAttribute(*itControllers, "id", strControllerId);
      if(strControllerId == str_controller_id) {
         strControllerTag = itControllers->Value();
         m_ptControllerConfRoot = &(*itControllers);
      }
   }
   std::cout<<strControllerTag;

   /* Make sure we found the tag */
   if(strControllerTag == "") {
      THROW_ARGOSEXCEPTION("Can't find controller with id \"" << str_controller_id << "\"");
   }

   /*
    * get connection to Thymio
    */
   try{
       this->ThymioInterface = ThymioInterface;
   }
   catch(CARGoSException& e)
   {
      THROW_ARGOSEXCEPTION("Error initializing communication with Thymio Interface");
   }

   /*
    * Initialize the robot
    */
   LOG << "[INFO] Robot initialization start" << std::endl;
   InitRobot();
   LOG << "[INFO] Robot initialization done" << std::endl;

   /*
    * Initialize the controller
    */
   LOG << "[INFO] Controller type '" << strControllerTag << "', id '" << str_controller_id << "' initialization start" << std::endl;
   m_pcController = CFactory<CCI_Controller>::New(strControllerTag);


   /* Set the controller id using the machine hostname */
   char pchHostname[256];
   pchHostname[255] = '\0';
   ::gethostname(pchHostname, 255);
   m_pcController->SetId(pchHostname);
   LOG << "[INFO] Controller id set to '" << pchHostname << "'" << std::endl;

   /* Go through actuators */
   TConfigurationNode& tActuators = GetNode(*m_ptControllerConfRoot, "actuators");
   TConfigurationNodeIterator itAct;
   for(itAct = itAct.begin(&tActuators);
       itAct != itAct.end();
       ++itAct) {
      /* itAct->Value() is the name of the current actuator */
      CCI_Actuator* pcCIAct = MakeActuator(itAct->Value());
      if(pcCIAct == NULL) {
         THROW_ARGOSEXCEPTION("Unknown actuator \"" << itAct->Value() << "\"");
      }
      pcCIAct->Init(*itAct);
      m_pcController->AddActuator(itAct->Value(), pcCIAct);
   }
   
   /* Go through sensors */
   TConfigurationNode& tSensors = GetNode(*m_ptControllerConfRoot, "sensors");
   TConfigurationNodeIterator itSens;
   for(itSens = itSens.begin(&tSensors);
       itSens != itSens.end();
       ++itSens) {
      /* itSens->Value() is the name of the current sensor */
      CCI_Sensor* pcCISens = MakeSensor(itSens->Value());
      if(pcCISens == NULL) {
         THROW_ARGOSEXCEPTION("Unknown sensor \"" << itSens->Value() << "\"");
      }
      pcCISens->Init(*itSens);
      m_pcController->AddSensor(itSens->Value(), pcCISens);
   }

   /* Configure the controller */
   m_pcController->Init(GetNode(*m_ptControllerConfRoot, "params"));
   LOG << "[INFO] Controller type '" << strControllerTag << "', id '" << str_controller_id << "' initialization done" << std::endl;
   /* Start the robot */
//   state = 1;
}

/****************************************/
/****************************************/

CRealRobot::~CRealRobot() {
   if(m_pcController)
      delete m_pcController;
}

/****************************************/
/****************************************/

void CRealRobot::Control() {
   m_pcController->ControlStep();
}

/****************************************/
/****************************************/

void CRealRobot::Execute() {
    Real passed_time = 0;
   /* Enforce the control rate */
   CRate cRate(m_fRate);
   /* Main loop */
   LOG << "[INFO] Control loop running" << std::endl;
   bool count = true;
   while(count) {
      /* Do useful work */
      Sense();
      Control();
      Act();

      /* Sleep to enforce control rate */
      cRate.Sleep();

      /*enforce the timer*/
      passed_time++;
      if(passed_time/cRate.GetRate() >= this->m_Time && this->m_Time!=0.0f)
          count = false;
   }
   this->Cleanup(1);
}

/****************************************/
/****************************************/

void CRealRobot::Cleanup(int) {

   LOG << "[INFO] Stopping controller" << std::endl;
   if(m_pcInstance != NULL) {
      m_pcInstance->Destroy();
      delete m_pcInstance;
   }
   LOG << "[INFO] All done" << std::endl;
   exit(0);
}

/****************************************/
/****************************************/



