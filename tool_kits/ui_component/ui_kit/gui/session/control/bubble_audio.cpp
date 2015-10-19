﻿#include "bubble_audio.h"
#include "callback/audio/audio_callback.h"
#include "util/user.h"

using namespace ui;

namespace nim_comp
{
MsgBubbleAudio::MsgBubbleAudio()
{
	is_playing_ = false;

	time_ = 0;
	tick_ = 0;
}

void MsgBubbleAudio::InitControl(bool bubble_right)
{
	__super::InitControl(bubble_right);

	msg_audio_ = new ButtonBox;
	if(bubble_right)
		GlobalManager::FillBoxWithCache(msg_audio_, L"session/audio_right.xml");
	else
		GlobalManager::FillBoxWithCache(msg_audio_, L"session/audio_left.xml");
	bubble_box_->Add(msg_audio_);

	audio_icon_ = msg_audio_->FindSubControl( L"audio_icon" );
	play_icon_ = msg_audio_->FindSubControl( L"play_icon" );

	tip_time_ = (Label*) msg_audio_->FindSubControl( L"tip_time" );

	msg_audio_->AttachBubbledEvent(ui::kEventClick, nbase::Bind(&MsgBubbleAudio::OnClicked, this, std::placeholders::_1));
	msg_audio_->AttachMenu(nbase::Bind(&MsgBubbleAudio::OnMenu, this, std::placeholders::_1));
}

void MsgBubbleAudio::InitInfo(const MsgData &msg)
{
	__super::InitInfo(msg);

	SetCanPlay(false);
	InitTime();

	path_ = msg.local_file_path;
	if( path_.empty() )
	{
		Json::Value value;
		if( StringToJson(msg.msg_attach, value) )
		{
			std::string md5 = value[nim::kNIMAudioMsgKeyMd5].asString();
			std::string dir = nbase::UTF16ToUTF8(GetUserAudioPath());
			path_  = dir + md5;
		}
		else
		{
			QLOG_ERR(L"parse audio msg attach fail: {0}") <<msg.msg_attach;
			return;
		}
	}

	if( shared::FilePathIsExist(path_, false) )
	{
		SetCanPlay(true);
	}
	else
	{
		if(msg_.msg_code == 0) //读取消息历史
		{
			SetLoadStatus(RS_LOADING);
		}
		else //接收
		{
			if (msg_.msg_code == nim::kNIMResSuccess)
				SetLoadStatus(RS_LOADING);
			else if (msg_.msg_code == nim::kNIMLocalResMsgUrlInvalid)
				SetLoadStatus(RS_LOAD_NO);
			else if (msg_.msg_code == nim::kNIMLocalResMsgFileExist)
				SetCanPlay(true);
			else
				QLOG_WAR(L"unknown receive msg code {0}") <<msg_.msg_code;
		}
	}
	SetPlayed(msg_.msg_sub_status == nim::kNIMMsgLogSubStatusPlayed);
}

bool MsgBubbleAudio::OnClicked( ui::EventArgs* arg )
{
	std::wstring name = arg->pSender->GetName();
	if( name == L"msg_audio" )
	{
		if(is_playing_)
		{
			nim::Audio::StopPlayAudio();
		}
		else
		{
			if( !shared::FilePathIsExist(path_, false) )
			{
				QLOG_ERR(L"Audio not exist: {0}") <<path_;
				return true;
			}

			AudioCallback::SetPlaySid(sid_);
			AudioCallback::SetPlayCid(msg_.client_msg_id);

			nim::Audio::PlayAudio(path_.c_str(), sid_.c_str(), msg_.client_msg_id.c_str());
			nim::MsgLog::SetSubStatusAsync(msg_.client_msg_id, nim::kNIMMsgLogSubStatusPlayed, nim::MsgLog::SetSubStatusCallback());
			msg_.msg_sub_status = nim::kNIMMsgLogSubStatusPlayed;
			SetPlayed(true);
		}
	}
	return true;
}

void MsgBubbleAudio::OnTick()
{
	tick_++;
	SetTime(tick_);
}

void MsgBubbleAudio::InitTime()
{
	Json::Value value;
	if( StringToJson(msg_.msg_attach, value) )
	{
		int t = value[nim::kNIMAudioMsgKeyDuration].asInt();
		time_ = t / 1000;
		if(t % 1000 >= 500)
			time_++;
		SetTime(time_);

		Box* audio_play_box = (Box*)msg_audio_->FindSubControl(L"audio_play_box");
		ui::UiRect margin = audio_play_box->GetMargin();
		if (IsBubbleRight(msg_))
			margin.left += time_;
		else
			margin.right += time_;
		audio_play_box->SetMargin(margin);
	}
	else
	{
		QLOG_ERR(L"parse audio msg attach error: {0}") <<msg_.msg_attach;
	}
}

void MsgBubbleAudio::SetTime( int sec )
{
	std::wstring str = nbase::StringPrintf(L"%d\"", sec);
	tip_time_->SetText(str);
}

void MsgBubbleAudio::SetCanPlay( bool can )
{
	msg_audio_->SetEnabled(can);
}

void MsgBubbleAudio::DoPlay()
{
	is_playing_ = true;

	audio_icon_->SetVisible( false );
	play_icon_->SetVisible( true );

	tick_ = 0;
	SetTime(tick_);
	StdClosure cb = nbase::Bind(&MsgBubbleAudio::OnTick, this);
	auto weak_cb = timer_.ToWeakCallback(cb);
	nbase::ThreadManager::PostRepeatedTask(weak_cb, nbase::TimeDelta::FromSeconds(1));
}

void MsgBubbleAudio::DoStop()
{
	is_playing_ = false;

	play_icon_->SetVisible( false );
	audio_icon_->SetVisible( true );

	SetTime(time_);
	timer_.Cancel();
}

void MsgBubbleAudio::OnDownloadCallback( bool success )
{
	if( success )
	{
		SetLoadStatus(RS_LOAD_OK);
		SetCanPlay(true);
	}
	else
	{
		SetLoadStatus(RS_RELOAD);
	}
}

void MsgBubbleAudio::OnPlayCallback( int code )
{
	if (code == nim::kSuccess)
	{
		DoPlay();
	}
}

void MsgBubbleAudio::OnStopCallback( int code )
{
	DoStop();
}


bool MsgBubbleAudio::OnMenu( ui::EventArgs* arg )
{
	PopupMenu(false);
	return false;
}

bool MsgBubbleAudio::NeedDownloadResource()
{
	if( shared::FilePathIsExist(path_, false) )
		return false;
	else
		return true;
}
}