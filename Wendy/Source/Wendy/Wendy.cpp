// Copyright Working Dungeon Wendy, by DJ Song

#include "Wendy.h"
#include "WendyCommon.h"
#include "Modules/ModuleManager.h"
#include <Windows.h>

IMPLEMENT_PRIMARY_GAME_MODULE(FWendyGameModule, Wendy, "Wendy" );
 
DEFINE_LOG_CATEGORY(LogWendy);

void FWendyGameModule::StartupModule()
{

}

void FWendyGameModule::ShutdownModule()
{

}

/* Put some definition of util declared in WendyCommon.h */
EWendyRemoteInputKeys FromFKeyToWendyRemoteKey(const FKey InFKey)
{
	if (InFKey == EKeys::BackSpace)
	{
		return EWendyRemoteInputKeys::Key_BackSpace;
	}
	else if (InFKey == EKeys::Tab)
	{
		return EWendyRemoteInputKeys::Key_Tab;
	}
	else if (InFKey == EKeys::Enter)
	{
		return EWendyRemoteInputKeys::Key_Enter;
	}
	else if (InFKey == EKeys::Escape)
	{
		return EWendyRemoteInputKeys::Key_Escape;
	}
	else if (InFKey == EKeys::SpaceBar)
	{
		return EWendyRemoteInputKeys::Key_SpaceBar;
	}
	else if (InFKey == EKeys::PageUp)
	{
		return EWendyRemoteInputKeys::Key_PageUp;
	}
	else if (InFKey == EKeys::PageDown)
	{
		return EWendyRemoteInputKeys::Key_PageDown;
	}
	else if (InFKey == EKeys::Left)
	{
		return EWendyRemoteInputKeys::Key_Left;
	}
	else if (InFKey == EKeys::Up)
	{
		return EWendyRemoteInputKeys::Key_Up;
	}
	else if (InFKey == EKeys::Right)
	{
		return EWendyRemoteInputKeys::Key_Right;
	}
	else if (InFKey == EKeys::Down)
	{
		return EWendyRemoteInputKeys::Key_Down;
	}
	else if (InFKey == EKeys::Delete)
	{
		return EWendyRemoteInputKeys::Key_Delete;
	}
	else if (InFKey == EKeys::Zero)
	{
		return EWendyRemoteInputKeys::Key_Zero;
	}
	else if (InFKey == EKeys::One)
	{
		return EWendyRemoteInputKeys::Key_One;
	}
	else if (InFKey == EKeys::Two)
	{
		return EWendyRemoteInputKeys::Key_Two;
	}
	else if (InFKey == EKeys::Three)
	{
		return EWendyRemoteInputKeys::Key_Three;
	}
	else if (InFKey == EKeys::Four)
	{
		return EWendyRemoteInputKeys::Key_Four;
	}
	else if (InFKey == EKeys::Five)
	{
		return EWendyRemoteInputKeys::Key_Five;
	}
	else if (InFKey == EKeys::Six)
	{
		return EWendyRemoteInputKeys::Key_Six;
	}
	else if (InFKey == EKeys::Seven)
	{
		return EWendyRemoteInputKeys::Key_Seven;
	}
	else if (InFKey == EKeys::Eight)
	{
		return EWendyRemoteInputKeys::Key_Eight;
	}
	else if (InFKey == EKeys::Nine)
	{
		return EWendyRemoteInputKeys::Key_Nine;
	}
	else if (InFKey == EKeys::F1)
	{
		return EWendyRemoteInputKeys::Key_F1;
	}
	else if (InFKey == EKeys::F2)
	{
		return EWendyRemoteInputKeys::Key_F2;
	}
	else if (InFKey == EKeys::F3)
	{
		return EWendyRemoteInputKeys::Key_F3;
	}
	else if (InFKey == EKeys::F4)
	{
		return EWendyRemoteInputKeys::Key_F4;
	}
	else if (InFKey == EKeys::F5)
	{
		return EWendyRemoteInputKeys::Key_F5;
	}
	else if (InFKey == EKeys::F6)
	{
		return EWendyRemoteInputKeys::Key_F6;
	}
	else if (InFKey == EKeys::F7)
	{
		return EWendyRemoteInputKeys::Key_F7;
	}
	else if (InFKey == EKeys::F8)
	{
		return EWendyRemoteInputKeys::Key_F8;
	}
	else if (InFKey == EKeys::F9)
	{
		return EWendyRemoteInputKeys::Key_F9;
	}
	else if (InFKey == EKeys::F10)
	{
		return EWendyRemoteInputKeys::Key_F10;
	}
	else if (InFKey == EKeys::F11)
	{
		return EWendyRemoteInputKeys::Key_F11;
	}
	else if (InFKey == EKeys::F12)
	{
		return EWendyRemoteInputKeys::Key_F12;
	}
	else if (InFKey == EKeys::A)
	{
		return EWendyRemoteInputKeys::Key_A;
	}
	else if (InFKey == EKeys::B)
	{
		return EWendyRemoteInputKeys::Key_B;
	}
	else if (InFKey == EKeys::C)
	{
		return EWendyRemoteInputKeys::Key_C;
	}
	else if (InFKey == EKeys::D)
	{
		return EWendyRemoteInputKeys::Key_D;
	}
	else if (InFKey == EKeys::E)
	{
		return EWendyRemoteInputKeys::Key_E;
	}
	else if (InFKey == EKeys::F)
	{
		return EWendyRemoteInputKeys::Key_F;
	}
	else if (InFKey == EKeys::G)
	{
		return EWendyRemoteInputKeys::Key_G;
	}
	else if (InFKey == EKeys::H)
	{
		return EWendyRemoteInputKeys::Key_H;
	}
	else if (InFKey == EKeys::I)
	{
		return EWendyRemoteInputKeys::Key_I;
	}
	else if (InFKey == EKeys::J)
	{
		return EWendyRemoteInputKeys::Key_J;
	}
	else if (InFKey == EKeys::K)
	{
		return EWendyRemoteInputKeys::Key_K;
	}
	else if (InFKey == EKeys::L)
	{
		return EWendyRemoteInputKeys::Key_L;
	}
	else if (InFKey == EKeys::M)
	{
		return EWendyRemoteInputKeys::Key_M;
	}
	else if (InFKey == EKeys::N)
	{
		return EWendyRemoteInputKeys::Key_N;
	}
	else if (InFKey == EKeys::O)
	{
		return EWendyRemoteInputKeys::Key_O;
	}
	else if (InFKey == EKeys::P)
	{
		return EWendyRemoteInputKeys::Key_P;
	}
	else if (InFKey == EKeys::Q)
	{
		return EWendyRemoteInputKeys::Key_Q;
	}
	else if (InFKey == EKeys::R)
	{
		return EWendyRemoteInputKeys::Key_R;
	}
	else if (InFKey == EKeys::S)
	{
		return EWendyRemoteInputKeys::Key_S;
	}
	else if (InFKey == EKeys::T)
	{
		return EWendyRemoteInputKeys::Key_T;
	}
	else if (InFKey == EKeys::U)
	{
		return EWendyRemoteInputKeys::Key_U;
	}
	else if (InFKey == EKeys::V)
	{
		return EWendyRemoteInputKeys::Key_V;
	}
	else if (InFKey == EKeys::W)
	{
		return EWendyRemoteInputKeys::Key_W;
	}
	else if (InFKey == EKeys::X)
	{
		return EWendyRemoteInputKeys::Key_X;
	}
	else if (InFKey == EKeys::Y)
	{
		return EWendyRemoteInputKeys::Key_Y;
	}
	else if (InFKey == EKeys::Z)
	{
		return EWendyRemoteInputKeys::Key_Z;
	}

	return EWendyRemoteInputKeys::None;
}
uint8 FromWendyRemoteKeyToWinVK(EWendyRemoteInputKeys InRemoteInputKey)
{
#if PLATFORM_WINDOWS
	switch (InRemoteInputKey)
	{
	case EWendyRemoteInputKeys::Key_BackSpace :
		return VK_BACK;
	case EWendyRemoteInputKeys::Key_Tab:
		return VK_TAB;
	case EWendyRemoteInputKeys::Key_Enter:
		return VK_RETURN;
	case EWendyRemoteInputKeys::Key_Escape:
		return VK_ESCAPE;
	case EWendyRemoteInputKeys::Key_SpaceBar:
		return VK_SPACE;
	case EWendyRemoteInputKeys::Key_PageUp:
		return VK_PRIOR;
	case EWendyRemoteInputKeys::Key_PageDown:
		return VK_NEXT;
	case EWendyRemoteInputKeys::Key_Left:
		return VK_LEFT;
	case EWendyRemoteInputKeys::Key_Up:
		return VK_UP;
	case EWendyRemoteInputKeys::Key_Right:
		return VK_RIGHT;
	case EWendyRemoteInputKeys::Key_Down:
		return VK_DOWN;
	case EWendyRemoteInputKeys::Key_Delete:
		return VK_DELETE;
	case EWendyRemoteInputKeys::Key_Zero:
		return '0';
	case EWendyRemoteInputKeys::Key_One:
		return '1';
	case EWendyRemoteInputKeys::Key_Two:
		return '2';
	case EWendyRemoteInputKeys::Key_Three:
		return '3';
	case EWendyRemoteInputKeys::Key_Four:
		return '4';
	case EWendyRemoteInputKeys::Key_Five:
		return '5';
	case EWendyRemoteInputKeys::Key_Six:
		return '6';
	case EWendyRemoteInputKeys::Key_Seven:
		return '7';
	case EWendyRemoteInputKeys::Key_Eight:
		return '8';
	case EWendyRemoteInputKeys::Key_Nine:
		return '9';
	case EWendyRemoteInputKeys::Key_F1:
		return VK_F1;
	case EWendyRemoteInputKeys::Key_F2:
		return VK_F2;
	case EWendyRemoteInputKeys::Key_F3:
		return VK_F3;
	case EWendyRemoteInputKeys::Key_F4:
		return VK_F4;
	case EWendyRemoteInputKeys::Key_F5:
		return VK_F5;
	case EWendyRemoteInputKeys::Key_F6:
		return VK_F6;
	case EWendyRemoteInputKeys::Key_F7:
		return VK_F7;
	case EWendyRemoteInputKeys::Key_F8:
		return VK_F8;
	case EWendyRemoteInputKeys::Key_F9:
		return VK_F9;
	case EWendyRemoteInputKeys::Key_F10:
		return VK_F10;
	case EWendyRemoteInputKeys::Key_F11:
		return VK_F11;
	case EWendyRemoteInputKeys::Key_F12:
		return VK_F12;
	case EWendyRemoteInputKeys::Key_A:
		return 'A';
	case EWendyRemoteInputKeys::Key_B:
		return 'B';
	case EWendyRemoteInputKeys::Key_C:
		return 'C';
	case EWendyRemoteInputKeys::Key_D:
		return 'D';
	case EWendyRemoteInputKeys::Key_E:
		return 'E';
	case EWendyRemoteInputKeys::Key_F:
		return 'F';
	case EWendyRemoteInputKeys::Key_G:
		return 'G';
	case EWendyRemoteInputKeys::Key_H:
		return 'H';
	case EWendyRemoteInputKeys::Key_I:
		return 'I';
	case EWendyRemoteInputKeys::Key_J:
		return 'J';
	case EWendyRemoteInputKeys::Key_K:
		return 'K';
	case EWendyRemoteInputKeys::Key_L:
		return 'L';
	case EWendyRemoteInputKeys::Key_M:
		return 'M';
	case EWendyRemoteInputKeys::Key_N:
		return 'N';
	case EWendyRemoteInputKeys::Key_O:
		return 'O';
	case EWendyRemoteInputKeys::Key_P:
		return 'P';
	case EWendyRemoteInputKeys::Key_Q:
		return 'Q';
	case EWendyRemoteInputKeys::Key_R:
		return 'R';
	case EWendyRemoteInputKeys::Key_S:
		return 'S';
	case EWendyRemoteInputKeys::Key_T:
		return 'T';
	case EWendyRemoteInputKeys::Key_U:
		return 'U';
	case EWendyRemoteInputKeys::Key_V:
		return 'V';
	case EWendyRemoteInputKeys::Key_W:
		return 'W';
	case EWendyRemoteInputKeys::Key_X:
		return 'X';
	case EWendyRemoteInputKeys::Key_Y:
		return 'Y';
	case EWendyRemoteInputKeys::Key_Z:
		return 'Z';
	}
#endif

	return 0;
}
