/**
* \file semaphore.c
* \brief BRTOS Semaphore functions
*
* Functions to install and use semaphores
*
**/
/*********************************************************************************************************
*                                               BRTOS
*                                Brazilian Real-Time Operating System
*                            Acronymous of Basic Real-Time Operating System
*
*                              
*                                  Open Source RTOS under MIT License
*
*
*
*                                       OS Semaphore functions
*
*
*   Author:   Gustavo Weber Denardin
*   Revision: 1.0
*   Date:     11/03/2010
*
*   Authors:  Carlos Henrique Barriquelo e Gustavo Weber Denardin
*   Revision: 1.2
*   Date:     01/10/2010
*
*   Authors:  Carlos Henrique Barriquelo e Gustavo Weber Denardin
*   Revision: 1.3
*   Date:     11/10/2010
*
*   Authors:  Carlos Henrique Barriquelo e Gustavo Weber Denardin
*   Revision: 1.4
*   Date:     19/10/2010
*
*   Authors:  Carlos Henrique Barriquelo e Gustavo Weber Denardin
*   Revision: 1.45
*   Date:     20/10/2010
*
*********************************************************************************************************/

#include "OS_types.h"
#include "event.h"
#include "BRTOS.h"
#include "hardware.h"

#pragma warn_implicitconv off

#if (BRTOS_SEM_EN == 1)
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
/////      Create Semaphore Function                   /////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

INT8U OSSemCreate (INT8U cnt, BRTOS_Sem **event)
{
  #if (NESTING_INT == 1)
  INT16U CPU_SR = 0;
  #endif
  int i=0;

  BRTOS_Sem *pont_event;

  if (iNesting > 0) {                                // See if caller is an interrupt
     return(IRQ_PEND_ERR);                           // Can't be create by interrupt
  }
    
  // Enter critical Section
  if (currentTask)
     OSEnterCritical();
  
  // Verifica se ainda h� blocos de controle de eventos dispon�veis
  for(i=0;i<=BRTOS_MAX_SEM;i++)
  {
    
    if(i == BRTOS_MAX_SEM)
      // Caso n�o haja mais blocos dispon�veis, retorna exce��o
      return(NO_AVAILABLE_EVENT);
          
    
    if(BRTOS_Sem_Table[i].OSEventAllocated != TRUE)
    {
      BRTOS_Sem_Table[i].OSEventAllocated = TRUE;
      pont_event = &BRTOS_Sem_Table[i];
      break;      
    }
  }
  
    // Exit Critical
  pont_event->OSEventCount = cnt;                      // Set semaphore count value
  pont_event->OSEventWait  = 0;
  
  pont_event->OSEventWaitList=0;
  
  *event = pont_event;
  
  // Exit critical Section
  if (currentTask)
     OSExitCritical();
  
  return(ALLOC_EVENT_OK);
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
/////      Delete Semaphore Function                   /////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

INT8U OSSemDelete (BRTOS_Sem **event)
{
  #if (NESTING_INT == 1)
  INT16U CPU_SR = 0;
  #endif
  BRTOS_Sem *pont_event;

  if (iNesting > 0) {                                // See if caller is an interrupt
      return(IRQ_PEND_ERR);                          // Can't be delete by interrupt
  }
    
  // Enter Critical Section
  OSEnterCritical();
  
  pont_event = *event;  
  pont_event->OSEventAllocated = 0;
  pont_event->OSEventCount     = 0;                      
  pont_event->OSEventWait      = 0;   
  pont_event->OSEventWaitList=0;
  
  *event = NULL;
  
  // Exit Critical Section
  OSExitCritical();
  
  return(DELETE_EVENT_OK);
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
/////      Semaphore Pend Function                     /////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

INT8U OSSemPend (BRTOS_Sem *pont_event, INT16U timeout)
{
  #if (NESTING_INT == 1)
  INT16U CPU_SR = 0;
  #endif
  INT8U  iPriority = 0;
  ContextType *Task;
  
  #if ERROR_CHECK == 1
  if(pont_event == NULL){  
    return;
  }
  // Can not use semaphore pend function from interrupt handling code
  if(iNesting > 0)
  {
    return(IRQ_PEND_ERR);
  }
  #endif
  
  // BRTOS TRACE SUPPORT
  #if (OSTRACE == 1)     
      #if(OS_TRACE_BY_TASK == 1)
      Update_OSTrace(currentTask, SEMPEND);
      #else
      Update_OSTrace(ContextTask[currentTask].Priority, SEMPEND);
      #endif
  #endif  
  
      
  // Enter Critical Section
  OSEnterCritical(); 
  
  // Verify if there was a post
  if ( pont_event->OSEventCount > 0)
  {        
    // Decreases semaphore count 
    pont_event->OSEventCount--;
    
    // Exit Critical Section  
    OSExitCritical();
    
    return OK;
  } 
 
  Task = &ContextTask[currentTask];
    
  // Copy task priority to local scope
  iPriority = Task->Priority;
  // Increases the semaphore wait list counter
  pont_event->OSEventWait++;
  
  // Allocates the current task on the semaphore wait list
  pont_event->OSEventWaitList = pont_event->OSEventWaitList | (PriorityMask[iPriority]);
  
  // Task entered suspended state, waiting for semaphore post
  #if (VERBOSE == 1)
  Task->State = SUSPENDED;
  Task->SuspendedType = SEMAPHORE;
  #endif
  
  // Remove current task from the Ready List
  OSReadyList = OSReadyList ^ (PriorityMask[iPriority]);
  
  // Set timeout overflow
  if (timeout)
  {  
    timeout = counter + timeout;
    if (timeout >= TickCountOverFlow)
      timeout = timeout - TickCountOverFlow;
    
    Task->TimeToWait = timeout;
    
    // Put task into delay list
    if(Tail != NULL)
    { 
      // Insert task into list
      Tail->Next = Task;
      Task->Previous = Tail;
      Tail = Task;
      Tail->Next = NULL;
    }
    else
    {
       // Init delay list
       Tail = Task;
       Head = Task; 
    }
  } else
  {
    Task->TimeToWait = NO_TIMEOUT;
  }
  iNesting++;
  
  // Exit Critical Section
  OSExitCritical();
  
  
  // Enter Critical Section
  OSEnterCritical();
  
  iNesting--;
  
  // Change Context - Returns on time overflow or semaphore post
  ChangeContext();

  /*
  #if (NESTING_INT == 1)
  // Exit Critical Section
  OSExitCritical();
  #endif
  */
  
  
  // Enter Critical Section
  // OSEnterCritical();
  
  
  // Verify if the reason of task wake up was semaphore timeout
  if(Task->TimeToWait == EXIT_BY_TIMEOUT)
  {      
    // Remove the task from the semaphore wait list
    pont_event->OSEventWaitList = pont_event->OSEventWaitList ^ (PriorityMask[iPriority]);
    
    // Decreases the semaphore wait list counter
    pont_event->OSEventWait--;
    
    // Exit Critical Section
    OSExitCritical();
    
    // Indicates semaphore timeout
    return TIMEOUT;
  }
  // Remove the time to wait condition
  else
  {
    if (timeout)
    {
        Task->TimeToWait = NO_TIMEOUT;
        
        // Remove from delay list
        if(Task == Head)
        {
          Head = Task->Next;
          Head->Previous = NULL;
          if(Task == Tail)
          {
            Tail = Task->Previous;
            Tail->Next = NULL;
          }          
        }
        else
        {          
          if(Task == Tail)
          {
            Tail = Task->Previous;
            Tail->Next = NULL;
          }
          else
          {
            Task->Next->Previous = Task->Previous;
            Task->Previous->Next = Task->Next; 
          }
        }    
    }
  }
    
  // Exit Critical Section
  OSExitCritical();
  
  return OK;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
/////      Semaphore Post Function                     /////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

INT8U OSSemPost(BRTOS_Sem *pont_event)
{
  #if (NESTING_INT == 1)
  INT16U CPU_SR = 0;
  #endif
  INT8U iPriority = (INT8U)0;
  INT8U TaskSelect = 0;
  
  // Enter Critical Section
  #if (NESTING_INT == 0)
  if (!iNesting)
  #endif
     OSEnterCritical();
     
  // BRTOS TRACE SUPPORT
  #if (OSTRACE == 1)  
    if(!iNesting){ 
      #if(OS_TRACE_BY_TASK == 1)
      Update_OSTrace(currentTask, SEMPOST);
      #else
      Update_OSTrace(ContextTask[currentTask].Priority, SEMPOST);
      #endif
    }else{
      Update_OSTrace(0, SEMPOST);
    }
  #endif      
  
  // See if any task is waiting for semaphore
  if (pont_event->OSEventWait != 0)
  {
    // Selects the highest priority task
    iPriority = SAScheduler(pont_event->OSEventWaitList);
    TaskSelect = PriorityVector[iPriority];

    // Remove the selected task from the semaphore wait list
    pont_event->OSEventWaitList = pont_event->OSEventWaitList ^ (PriorityMask[iPriority]);
    
    // Decreases the semaphore wait list counter
    pont_event->OSEventWait--;
    
    // Put the selected task into Ready List
    #if (VERBOSE == 1)
    ContextTask[TaskSelect].State = READY;
    #endif
    
    OSReadyList = OSReadyList | (PriorityMask[iPriority]);
    
    // If outside of an interrupt service routine, change context to the highest priority task
    // If inside of an interrupt, the interrupt itself will change the context to the highest priority task
    if (!iNesting)
    {
      // Verify if there is a higher priority task ready to run
      ChangeContext();      
    }

    // Exit Critical Section
    #if (NESTING_INT == 0)
    if (!iNesting)
    #endif
      OSExitCritical();
    
    return OK;
  }
    
  // Make sure semaphore will not overflow
  if (pont_event->OSEventCount < 255)
  {
    // Increment semaphore count
    pont_event->OSEventCount++;
                         
    // Exit Critical Section
    #if (NESTING_INT == 0)
    if (!iNesting)
    #endif
       OSExitCritical();
    return OK;
  }
  else
  {
    // Exit Critical Section             
    #if (NESTING_INT == 0)
    if (!iNesting)
    #endif
       OSExitCritical();
    
    // Indicates semaphore overflow
    return ERR_SEM_OVF;
  }
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
#endif