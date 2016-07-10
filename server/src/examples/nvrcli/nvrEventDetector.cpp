#include "nvrEventDetector.h"

#include <base/ozMotionFrame.h>
#include <libgen/libgenTime.h>

/**
	This is a customization of ozEventRecorder --> it does the same thing,
	but also issues a callback when a new event is detected 

	This is actually a reasonable example to show you how you can write any custom
	Consumer (or any other Provider/Input/etc for that matter)
*/

// oZone expects a run method that does init for these classes. Write your own.
int EventDetector::run()
{
    if ( waitForProviders() ) // if there is no registerProvider for this class, no need to work
    {
        while( !mStop ) // set when you call ->stop()
        {

            mQueueMutex.lock();
			// will contain a queue of frames. In this example, since we connected EventDetector 
			// to MotionDetector as its provider, it will only be motion frames
            if ( !mFrameQueue.empty() )
            {
                for ( FrameQueue::iterator iter = mFrameQueue.begin(); iter != mFrameQueue.end(); iter++ )
                {
                    processFrame( *iter );
                }
                mFrameQueue.clear();
            }
            mQueueMutex.unlock();
            checkProviders();
            usleep( INTERFRAME_TIMEOUT );
        }
    }
    cleanup();
    return( 0 );
}

/**
* @brief 
*
* @param frame
*
* @return 
*/
bool EventDetector::processFrame( FramePtr frame )
{
    const MotionFrame *motionFrame = dynamic_cast<const MotionFrame *>(frame.get());

    AlarmState lastState = mState;
    uint64_t now = time64();

    if ( motionFrame->alarmed() )
    {
        mState = ALARM;
        mAlarmTime = now;
        if ( lastState == IDLE )
        {
            // Create new event
            mEventCount++;
			mFunction(mName); // issue callback
            for ( FrameStore::const_iterator iter = mFrameStore.begin(); iter != mFrameStore.end(); iter++ )
            {
                const MotionFrame *frame = dynamic_cast<const MotionFrame *>( iter->get() );
                std::string path = stringtf( "%s/img-%s-%d-%ju.jpg", mLocation.c_str(), mName.c_str(), mEventCount, frame->id() );
                //Info( "PF:%d @ %dx%d", frame->pixelFormat(), frame->width(), frame->height() );
                Image image( frame->pixelFormat(), frame->width(), frame->height(), frame->buffer().data() );
                image.writeJpeg( path.c_str() );
            }
        }
    }
    else if ( lastState == ALARM )
    {
        mState = ALERT;
    }
    else if ( lastState == ALERT )
    {
        if ( frame->age( mAlarmTime ) > MAX_EVENT_TAIL_AGE )
            mState = IDLE;
    }

    if ( mState > IDLE )
    {
        std::string path;
        if ( mState == ALARM )
        {
            path = stringtf( "%s/img-%s-%d-%ju-A.jpg", mLocation.c_str(), mName.c_str(), mEventCount, motionFrame->id() );
        }
        else if ( mState == ALERT )
        {
            path = stringtf( "%s/img-%s-%d-%ju.jpg", mLocation.c_str(), mName.c_str(), mEventCount, motionFrame->id() );
        }
        Info( "PF:%d @ %dx%d", motionFrame->pixelFormat(), motionFrame->width(), motionFrame->height() );
        Image image( motionFrame->pixelFormat(), motionFrame->width(), motionFrame->height(), motionFrame->buffer().data() );
        image.writeJpeg( path.c_str() );
    }

    // Clear out old frames
    Debug( 5, "Got %lu frames in store", mFrameStore.size() );
    while( !mFrameStore.empty() )
    {
        FramePtr tempFrame = *(mFrameStore.begin());
        Debug( 5, "Frame %ju age %.2lf", tempFrame->id(), tempFrame->age() );
        if ( tempFrame->age() <= MAX_EVENT_HEAD_AGE )
            break;
        Debug( 5, "Deleting" );
        //delete tempFrame;
        mFrameStore.pop_front();
    }
    mFrameStore.push_back( frame );
    mFrameCount++;
    return( true );
}