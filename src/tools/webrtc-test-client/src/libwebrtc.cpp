#include "libwebrtc.h"

#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <thread>

// #include <json.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <api/peer_connection_interface.h>
#include "api/create_peerconnection_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "api/video_codecs/video_encoder_factory_template_libaom_av1_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_encoder_factory_template_open_h264_adapter.h"
#include "pc/video_track_source.h"
#include <rtc_base/physical_socket_server.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#include <rtc_base/logging.h>
#include <api/rtc_error.h>
#include <api/jsep.h>
#include <rtc_base/ref_counted_object.h>
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "media/base/video_adapter.h"
#include "media/base/video_broadcaster.h"
#include "rtc_base/checks.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/synchronization/mutex.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"


// #include "rtc_base/critical_section.h"

namespace pywebrtc
{

  std::function<void(std::string)> offer_callback;
  std::function<void(std::string)> answer_callback;
  std::function<void(std::string,std::string)> python_log;
  std::function<void(std::string)> python_on_message;

  void set_logging(const std::function<void(std::string, std::string)> fn) {
      python_log = fn;
  }
  void set_on_message(const std::function<void(std::string)> fn) {
      python_on_message = fn;
  }

  const char kAudioLabel[] = "audio_label";
  const char kVideoLabel[] = "video_label";
  const char kStreamId[] = "stream_id";


  class DummyVideoSource : public webrtc::VideoTrackSource {
  public:
    DummyVideoSource() : VideoTrackSource(/*remote=*/false) {}

    ~DummyVideoSource() override = default;

    bool is_screencast() const override { return false; }
  };

  class FakeVideoTrackSource : public webrtc::VideoTrackSource {
  public:
    static rtc::scoped_refptr<FakeVideoTrackSource> Create(bool is_screencast) {
      return rtc::make_ref_counted<FakeVideoTrackSource>(is_screencast);
    }

    static rtc::scoped_refptr<FakeVideoTrackSource> Create() {
      return Create(false);
    }

    bool is_screencast() const override { return is_screencast_; }

    void InjectFrame(const webrtc::VideoFrame& frame) {
      video_broadcaster_.OnFrame(frame);
    }

  protected:
    explicit FakeVideoTrackSource(bool is_screencast)
        : webrtc::VideoTrackSource(false /* remote */), is_screencast_(is_screencast) {}
    ~FakeVideoTrackSource() override = default;

    rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
      return &video_broadcaster_;
    }

  private:
    const bool is_screencast_;
    rtc::VideoBroadcaster video_broadcaster_;
  };


  class Connection {
  public:
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection;
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel;
    std::string sdp_type;
    std::string ice_array;

    void onSuccessCSD(webrtc::SessionDescriptionInterface* desc) {
      peer_connection->SetLocalDescription(ssdo, desc);
      std::string sdp;
      desc->ToString(&sdp);
      //std::cout<<"SDP offer: "<<sdp<<std::endl;
      std::cout<<"SDP offer size: "  << sdp.length() <<std::endl;
      if (sdp_type == "Offer") {
          offer_callback(sdp);
      } else {
          answer_callback(sdp);
      }
    }

    // ICEを取得したら、表示用JSON配列の末尾に追加
    void onIceCandidate(const webrtc::IceCandidateInterface* candidate) {
      
      std::cout<<"Ice connection callback\n";
      std::string candidate_str;
      candidate->ToString(&candidate_str);
      std::cout<<candidate_str<<std::endl;
      ice_array = ice_array + candidate_str + "  ";
    }

    class PCO : public webrtc::PeerConnectionObserver {
    private:
      Connection& parent;

    public:
      PCO(Connection& parent) : parent(parent) {
      }
    
      void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {
        //python_log("debug" , "PeerConnectionObserver::SignalingChange("+std::to_string(new_state)+")");
      };

      void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {
        //python_log("debug" , "PeerConnectionObserver::AddStream");
      };

      void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {
        //python_log("debug" , "PeerConnectionObserver::RemoveStream");
      };

      void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override {
          
        // std::cout << std::this_thread::get_id() << ":"
        //           << "PeerConnectionObserver::DataChannel(" << data_channel.get()
        //           << ", " << parent.data_channel.get() << ")" << std::endl;
        // parent.data_channel = data_channel;
        // parent.data_channel->RegisterObserver(&parent.dco);
      
      };

      void OnRenegotiationNeeded() override {
        std::cout<<"OnRenegotiationNeeded callback\n";
        //python_log("debug" , "PeerConnectionObserver::OnRenegotiationNeeded");
      };

      void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
        std::cout<<"OnIceConnectionChange callback\n";
          // I have no idea why, but uncommenting python_log make the program freeze
          //python_log("debug" , "PeerConnectionObserver::IceConnectionChange");
      };

      void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
        std::cout<<"OnIceGatheringChange callback\n";
          //python_log("debug" , "PeerConnectionObserver::IceGatheringChange");
      };

      void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
        //python_log("debug" , "PeerConnectionObserver::IceCandidate");
        std::cout<<"OnIceCandidate callback\n";
        parent.onIceCandidate(candidate);
      };
    };
    
    class DCO : public webrtc::DataChannelObserver {
    public:
      void OnStateChange() override {
          //python_log("debug" , "DataChannelObserver::StateChange");
      };
      
      void OnMessage(const webrtc::DataBuffer& buffer) override {
          python_on_message(std::string(buffer.data.data<char>(), buffer.data.size()));
      };

      void OnBufferedAmountChange(uint64_t previous_amount) override {
          //python_log("debug" , "DataChannelObserver::BufferedAmountChange");
      };
    };

    class CSDO : public webrtc::CreateSessionDescriptionObserver {
    private:
      Connection& parent;

    public:
      CSDO(Connection& parent) : parent(parent) {
      }
    
      void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
        //python_log("debug" , "CreateSessionDescriptionObserver::OnSuccess");
        std::cout << "CreateSessionDescriptionObserver::OnSuccess" << std::endl;
        
        parent.onSuccessCSD(desc);
      };

      void OnFailure(webrtc::RTCError error) override {
        //python_log("warn" , "CreateSessionDescriptionObserver::OnFailure\n"+ std::string(error.message()));
      };
    };

    class SSDO : public webrtc::SetSessionDescriptionObserver {
    public:
      void OnSuccess() override {
        
        //python_log("debug" , "SetSessionDescriptionObserver::OnSuccess");
      };

      void OnFailure(webrtc::RTCError error) override {
        //python_log("warn" , "SetSessionDescriptionObserver::OnFailure\n"+ std::string(error.message()));
      };
    };

    PCO  pco;
    DCO  dco;
    //rtc::scoped_refptr<CSDO> csdo;
    webrtc::CreateSessionDescriptionObserver* csdo;
    // rtc::scoped_refptr<SSDO> ssdo;
    webrtc::SetSessionDescriptionObserver* ssdo;
    rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface> local_observer;

    Connection() :
        pco(*this),
        dco(),
        csdo(new rtc::RefCountedObject<CSDO>(*this)),
        ssdo(new rtc::RefCountedObject<SSDO>()) {
    }
  };

  std::unique_ptr<rtc::Thread> network_thread;
  std::unique_ptr<rtc::Thread> signaling_thread;
  std::unique_ptr<rtc::Thread> worker_thread;
  //auto network_thread = rtc::Thread::Create();
  //auto worker_thread = rtc::Thread::Create();
  //auto signaling_thread = rtc::Thread::Create();
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory;
  webrtc::PeerConnectionInterface::RTCConfiguration configuration;
  Connection connection;

  void AddTracks() {
    if (!connection.peer_connection->GetSenders().empty()) {
      return;  // Already added tracks.
    }

    RTC_LOG(LS_ERROR) << "AddTracks() 1 ";
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
        peer_connection_factory->CreateAudioTrack(
            kAudioLabel,
            peer_connection_factory->CreateAudioSource(cricket::AudioOptions())
                .get()));
    RTC_LOG(LS_ERROR) << "AddTracks() 2 ";
    auto result_or_error = connection.peer_connection->AddTrack(audio_track, {kStreamId});
    if (!result_or_error.ok()) {
      RTC_LOG(LS_ERROR) << "Failed to add audio track to PeerConnection: "
                        << result_or_error.error().message();
    }
    RTC_LOG(LS_ERROR) << "AddTracks() 3";
  
    // rtc::scoped_refptr<FakeVideoTrackSource> video_device =
    //     FakeVideoTrackSource::Create();
    // // auto video_device = webrtc::VideoCaptureFactory::Create(0);
    // if (video_device) {
    //   // std::unique_ptr<cricket::VideoCapturer> capturer; //create capturer
    //   // rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource = peer_connection_factory->CreateVideoSource(std::move(capturer), NULL); //create video source from capturer

    //   // rtc::scoped_refptr<DummyVideoSource> videoSource = rtc::make_ref_counted<DummyVideoSource>(); //create dummy video source
    //   rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_(
    //       peer_connection_factory->CreateVideoTrack(video_device, kVideoLabel));

    //   result_or_error = connection.peer_connection->AddTrack(video_track_, {kStreamId});
    //   if (!result_or_error.ok()) {
    //     RTC_LOG(LS_ERROR) << "Failed to add video track to PeerConnection: "
    //                       << result_or_error.error().message();
    //   }
    // } else {
    //   RTC_LOG(LS_ERROR) << "OpenVideoCaptureDevice failed";
    // }

    // rtc::scoped_refptr<FakeVideoTrackSource> video_device =
    //     FakeVideoTrackSource::Create();
    // if (video_device) {
    //   rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_(
    //       peer_connection_factory->CreateVideoTrack(video_device, kVideoLabel));

    //   result_or_error = connection.peer_connection->AddTrack(video_track_, {kStreamId});
    //   if (!result_or_error.ok()) {
    //     RTC_LOG(LS_ERROR) << "Failed to add video track to PeerConnection: "
    //                       << result_or_error.error().message();
    //   }
    // } else {
    //   RTC_LOG(LS_ERROR) << "OpenVideoCaptureDevice failed";
    // }
 
  }

  void create_offer(std::function<void(std::string)> callback) {
    std::cout<<"create_offer called from webrtc"<<std::endl;
    offer_callback = callback;
    std::cout<<"callback initialised"<<std::endl;
    
    connection.sdp_type = "Offer";  
    std::cout<<"calling create_offer"<<std::endl;
    connection.peer_connection->CreateOffer(connection.csdo, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    std::cout<<"Offer created"<<std::endl;
    
  }

  void create_answer(const std::string& offer, std::function<void(std::string)> callback) {
    answer_callback = callback;
    // connection.peer_connection =
    //     peer_connection_factory->CreatePeerConnection(configuration, nullptr,
    //             nullptr, &connection.pco);

    // if (connection.peer_connection.get() == nullptr) {
    //   peer_connection_factory = nullptr;
    //   //python_log("warn", "Error on CreatePeerConnection.");
    //   return;
    // }
    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface* session_description(
        webrtc::CreateSessionDescription("offer", offer, &error));
    if (session_description == nullptr) {
      //python_log("warn", "Error on CreateSessionDescription.\n" + error.line + "\n" +  error.description);
    }
    /*
    connection.peer_connection->SetRemoteDescription(session_description, connection.ssdo);

    connection.sdp_type = "Answer"; // 表示用の文字列、webrtcの動作には関係ない
    connection.peer_connection->CreateAnswer(connection.csdo,
                                            webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
              */
  }

  void set_answer(const std::string& answer) {
    webrtc::SdpParseError error;
    webrtc::SessionDescriptionInterface* session_description(
        webrtc::CreateSessionDescription("answer", answer, &error));
    if (session_description == nullptr) {
      std::cout<<"Answer unable to set\n";
      //python_log("warn", "Error on CreateSessionDescription.\n" + error.line + "\n" +  error.description);
    }
    
    connection.peer_connection->SetRemoteDescription(
            connection.ssdo, session_description);
    std::cout<<"Answer set\n";
    
  }

  void set_candidates(const std::string& candidates) {
    
    return;
  }

  std::string get_candidates() {
  
    return connection.ice_array;
  }

  void send_data(const std::string& data) {
    webrtc::DataBuffer buffer(rtc::CopyOnWriteBuffer(data.c_str(), data.size()), true);
    connection.data_channel->Send(buffer);
  }

  void destructor() {
    rtc::CleanupSSL();
    connection.peer_connection->Close();
    connection.peer_connection = nullptr;
    connection.data_channel = nullptr;
    peer_connection_factory = nullptr;
    network_thread->Stop();
    worker_thread->Stop();
    signaling_thread->Stop();
  }

  int initialize () {
    rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
      std::cout << std::this_thread::get_id() << ":"
              << "Main thread" << std::endl;

    std::cout << "configuring ice servers" << std::endl;
    webrtc::PeerConnectionInterface::IceServer ice_server;
    ice_server.uri = "stun:stun.l.google.com:19302";
    configuration.servers.push_back(ice_server);

    std::cout << "Initialising SSL" << std::endl;
    rtc::InitializeSSL();
    network_thread = rtc::Thread::Create();
    //network_thread = rtc::Thread::CreateWithSocketServer();
    network_thread->Start();
    worker_thread = rtc::Thread::Create();
    worker_thread->Start();
    //signaling_thread = rtc::Thread::Create();
    signaling_thread = rtc::Thread::CreateWithSocketServer();
    signaling_thread->Start();

    std::cout << "creating PeerConnectionFactory" << std::endl;
    peer_connection_factory = webrtc::CreatePeerConnectionFactory(
        nullptr, nullptr,
        signaling_thread.get(), nullptr /* default_adm */,
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        /*
        webrtc::CreateBuiltinVideoEncoderFactory(),
      */
        std::make_unique<webrtc::VideoEncoderFactoryTemplate<
            webrtc::LibvpxVp8EncoderTemplateAdapter,
            webrtc::LibvpxVp9EncoderTemplateAdapter,
            webrtc::OpenH264EncoderTemplateAdapter,
            webrtc::LibaomAv1EncoderTemplateAdapter>>(),
        std::make_unique<webrtc::VideoDecoderFactoryTemplate<
            webrtc::LibvpxVp8DecoderTemplateAdapter,
            webrtc::LibvpxVp9DecoderTemplateAdapter,
            webrtc::OpenH264DecoderTemplateAdapter,
            webrtc::Dav1dDecoderTemplateAdapter>>(),
        nullptr /* audio_mixer */, nullptr /* audio_processing */);

    std::cout << "PeerConnectionFactory created" << std::endl;
  /*
    peer_connection_factory = webrtc::CreatePeerConnectionFactory(
        network_thread.get(),
        worker_thread.get(),
        signaling_thread.get(),
        nullptr ,
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        webrtc::CreateBuiltinVideoEncoderFactory(),
        webrtc::CreateBuiltinVideoDecoderFactory(),
        nullptr ,
        nullptr );
  */
    if (peer_connection_factory.get() == nullptr) {
      std::cout << "Error on CreatePeerConnectionFactory." << std::endl;
      return EXIT_FAILURE;
    }

    configuration.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    //connection.peer_connection = peer_connection_factory->CreatePeerConnection(
    //            configuration, nullptr, nullptr, &connection.pco);
    webrtc::PeerConnectionDependencies pc_dependencies(&connection.pco);
    auto error_or_peer_connection =
        peer_connection_factory->CreatePeerConnectionOrError(
            configuration, std::move(pc_dependencies));
    if (error_or_peer_connection.ok()) {
      connection.peer_connection = std::move(error_or_peer_connection.value());
    }
    std::cout<<"peer connection initialised"<<std::endl;

    webrtc::DataChannelInit config;
    connection.data_channel = connection.peer_connection->CreateDataChannel("data_channel", &config);
    connection.data_channel->RegisterObserver(&connection.dco);
    std::cout<<"data channel created"<<std::endl;

    if (connection.peer_connection.get() == nullptr) {
      peer_connection_factory = nullptr;
      //python_log("warn", "Error on CreatePeerConnection.");
      return 0;
    }
    AddTracks();

  }
}
