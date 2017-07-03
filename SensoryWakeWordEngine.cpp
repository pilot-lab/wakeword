/**
 * Copyright 2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Amazon Software License (the "License"). You may not use this file 
 * except in compliance with the License. A copy of the License is located at
 *
 *   http://aws.amazon.com/asl/
 *
 * or in the "license" file accompanying this file. This file is distributed on an "AS IS" 
 * BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, express or implied. See the License 
 * for the specific language governing permissions and limitations under the License.
 */

#include "SensoryWakeWordEngine.h"
#include "Logger.h"
#include "WakeWordUtils.h"
#include "WakeWordException.h"
#include <iostream>

const std::string ALEXA_TASK_VERSION = "~0.7.0";
const std::string MODEL_FILE = "../ext/resources/spot-alexa-rpi.snsr";

#define NETFILE      "/pilot-app/pilot-config/wakeword/alexa/thfft_alexa_enus_v1_524kb_am.raw"
#define SEARCHFILE   "/pilot-app/pilot-config/wakeword/alexa/thfft_alexa_enus_v1_524kb_search_14.raw" // pre-built search
#define NBEST              (1)                /* Number of results */
#define MAXSTR             (512)              /* Output string size */
#define SHOWAMP            (0)                /* Display amplitude */

#define SAMPLE_RATE	16000
#define CHANNELS	1
#define FILE_FORMAT	(SF_FORMAT_WAV | SF_FORMAT_PCM_16)
#define SAMPLES_PRE_FRAME 160

namespace AlexaWakeWord {

SensoryWakeWordEngine::SensoryWakeWordEngine(
        WakeWordDetectedInterface* interface) :
        WakeWordEngine(interface), m_isRunning{false}, m_session{nullptr}, m_recog{nullptr}, m_searchs{nullptr}, pa_record{nullptr} {

	try
	{
		init();
		start();
	} catch(std::bad_alloc& e) {
		log(Logger::ERROR, "SensoryWakeWordEngine: Failed to allocate memory");
		throw;
	} catch(WakeWordException& e) {
		log(Logger::ERROR,
		std::string("SensoryWakeWordEngine: Initialization error:") + e.what());
		throw;
	}
}

SensoryWakeWordEngine::~SensoryWakeWordEngine() {
	stop();
	thfRecogDestroy(m_recog);
	thfSearchDestroy(m_searchs);
	thfSessionDestroy(m_session);
	m_recog		= nullptr;
	m_searchs	= nullptr;
	m_session	= nullptr;
}

void SensoryWakeWordEngine::pause() {
	log(Logger::INFO, "SensoryWakeWordEngine: handling pause");
	stop();
}

void SensoryWakeWordEngine::resume() {

	log(Logger::INFO, "SensoryWakeWordEngine: handling resume");

	try {
		start();
	} catch (std::bad_alloc& e) {
		log(Logger::ERROR, "SensoryWakeWordEngine: Failed to allocate memory");
		throw;
	} catch (WakeWordException& e) {
		log(Logger::ERROR,
		std::string("SensoryWakeWordEngine: Initialization error:") + e.what());
		throw;
	}
}

bool SensoryWakeWordEngine::isRunning() {
  return m_isRunning;
}

void SensoryWakeWordEngine::callWakeWordDetected() {
  wakeWordDetected();
}

void SensoryWakeWordEngine::init() {

	if(m_session) {
		log(Logger::INFO, "SensoryWakeWordEngine already initialized");
		return;
	}

	log(Logger::INFO, std::string("Initializing Sensory library") +
								" | NETFILE: " + NETFILE +
								" | SEARCHFILE: " + SEARCHFILE);

	m_session	= thfSessionCreate();
	m_recog		= thfRecogCreateFromFile(m_session, NETFILE, (unsigned short)(SAMPLES_PRE_FRAME/1000.f*SAMPLE_RATE), -1, NO_SDET);
	m_searchs	= thfSearchCreateFromFile(m_session, m_recog, SEARCHFILE, NBEST);
	
	thfPhrasespotConfigSet(m_session, m_recog, m_searchs, PS_DELAY, PHRASESPOT_DELAY_ASAP);
	
	if(thfRecogGetSampleRate(m_session, m_recog) != SAMPLE_RATE)
	{
		log(Logger::INFO, "Sensory wrang SAMPLE_RATE");
	}
	
	thfRecogInit(m_session, m_recog, m_searchs, RECOG_KEEP_NONE);
}

void SensoryWakeWordEngine::start() {

	if(m_isRunning) {
		log(Logger::INFO, "SensoryWakeWordEngine already started");
		return;
	}
	log(Logger::DEBUG, "SensoryWakeWordEngine: starting");

	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = SAMPLE_RATE,
		.channels = CHANNELS
	};
	
	int error;
    if(!(pa_record = pa_simple_new(NULL, "alexa", PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error)))
	{
		log(Logger::INFO, "pa_simple_new() failed!");
        return;
    }
	
	m_isRunning = true;
	m_thread = make_unique<std::thread>(&SensoryWakeWordEngine::mainLoop, this);
}

void SensoryWakeWordEngine::stop() {

	if(!m_isRunning) {
		log(Logger::INFO, "SensoryWakeWordEngine already stopped");
		return;
	}
	log(Logger::INFO, " *** THREAD JOINING: Sensory ***");

	m_isRunning = false;
	m_thread->join();
	pa_simple_free(pa_record);
}

void SensoryWakeWordEngine::mainLoop() {
	
	using std::cout;
	using std::endl;
	log(Logger::INFO, "SensoryWakeWordEngine: mainLoop thread started");
	bool done = false;
	unsigned short status = 0;
	const char *rres;
	float score;
	
	short buf[SAMPLES_PRE_FRAME];
	int error = 0;
	
	while(m_isRunning == true)
	{
		cout << ">>> Say 'Alexa' <<<" << endl;

		/* Pipelined recognition */
		done = false;

		while(!done && m_isRunning == true)
		{
			if(pa_simple_read(pa_record, buf, sizeof(buf), &error) < 0)
			{
				cout << "pa_simple_read() failed!" << endl;
				continue;
			}
		
			if (!thfRecogPipe(m_session, m_recog, SAMPLES_PRE_FRAME, buf, RECOG_ONLY, &status))
				cout << "recogPipe!" << endl;
			if (status == RECOG_DONE)
			{
				done = true;
			}
		}

		/* Report N-best recognition result */
		rres=NULL;
		if (status==RECOG_DONE)
		{
			cout << "Recognition results..." << endl;
			score=0;
			if (!thfRecogResult(m_session, m_recog, &score, &rres, NULL, NULL, NULL, NULL, NULL, NULL)) 
				cout << "thfRecogResult" << endl;
			cout << "Result:" << rres << "(" << score << ")" << endl;
			callWakeWordDetected();
		}
		else
		{
			cout << "No recognition result" << endl;
		}
		thfRecogReset(m_session, m_recog);
	}

	log(Logger::INFO, "SensoryWakeWordEngine: mainLoop thread ended");
}

} // namespace AlexaWakeWord
