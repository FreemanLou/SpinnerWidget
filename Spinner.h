/*
	Spinner.h: A number spinner control.
	Written by DarkWyrm <darkwyrm@earthlink.net>, Copyright 2004
	Released under the MIT license.
	
	Original BScrollBarButton class courtesy Haiku project
*/
#ifndef SPINNER_H_
#define SPINNER_H_

#include <Control.h>
#include <TextView.h>
#include <Button.h>
#include <StringView.h>
#include <TextControl.h>

class SpinnerPrivateData;
class SpinnerArrowButton;
class SpinnerMsgFilter;

/*
	The Spinner control provides a numeric input which can be nudged by way of two
	small arrow buttons at the right side. The API is quite similar to that of
	BScrollBar.
*/

class Spinner : public BControl
{
public:
							Spinner(BRect frame, const char *name,
									const char *label, BMessage *msg,
									uint32 resize = B_FOLLOW_LEFT | B_FOLLOW_TOP,
									uint32 flags = B_WILL_DRAW | B_NAVIGABLE);
							Spinner(BMessage *data);
							Spinner(const char *name, const char *label, BMessage *msg,
									uint32 resize = B_FOLLOW_LEFT | B_FOLLOW_TOP, 
									uint32 flags = B_WILL_DRAW | B_NAVIGABLE);
	virtual					~Spinner(void);
	
	static	BArchivable*	Instantiate(BMessage *data);
	virtual	status_t		Archive(BMessage *data, bool deep = true) const;
		
	virtual	status_t		GetSupportedSuites(BMessage *msg);
	virtual BHandler*		ResolveSpecifier(BMessage *msg, int32 index,
											BMessage *specifier, int32 form,
											const char *property);
	
	virtual void			AttachedToWindow(void);
	virtual void			DetachedFromWindow(void);
	virtual void			ValueChanged(int32 value);
	virtual void			MessageReceived(BMessage *msg);
	
	virtual	void			GetPreferredSize(float *width, float *height);
	virtual void			ResizeToPreferred(void);
	
	virtual void			SetSteps(int32 stepsize);
			int32			GetSteps(void) const { return fStep; }
	
	virtual void			SetRange(int32 min, int32 max);
			void			GetRange(int32 *min, int32 *max);
	
	virtual	void			SetMax(int32 max);
			int32			GetMax(void) const { return fMax; }
	virtual	void			SetMin(int32 min);
			int32			GetMin(void) const { return fMin; }
	
	virtual	void			MakeFocus(bool value = true);
	
	virtual	void			SetValue(int32 value);
	virtual	void			SetLabel(const char *text);
			BTextControl*	TextControl(void) const { return fTextControl; }
	
	virtual	void			SetEnabled(bool value);
	virtual	void			SetDivider(float position);
			float			Divider() const;
			void			DoLayout();
			BLayoutItem*	CreateLabelLayoutItem();
			BLayoutItem*	CreateTextFieldLayoutItem();

private:
			class			LabelLayoutItem;
			class			TextFieldLayoutItem;
			struct			LayoutData;
			void			_InitObject(void);
			void			_UpdateFrame();
			void			_ValidateLayoutData();
			float			_TextFieldOffset();
			
	friend	class			SpinnerArrowButton;
	friend	class			SpinnerPrivateData;
	friend	class			LabelLayoutItem;
	friend	class			TextFieldLayoutItem;
	friend	struct			LayoutData;
	
	BTextControl*			fTextControl;
	SpinnerArrowButton*		fUpButton;
	SpinnerArrowButton*		fDownButton;
	SpinnerPrivateData*		fPrivateData;
	LayoutData*				fLayoutData;
	int32					fStep;
	int32					fMin;
	int32					fMax;
	float					fDivider;
	SpinnerMsgFilter		*fFilter;
			Spinner&			operator=(const Spinner& other);
};

#endif
