from django import forms
from django.contrib.auth.forms import AuthenticationForm


class PlayerAuthenticationForm(AuthenticationForm):
    remember_me = forms.BooleanField(
        required=False,
        label="Remember me for 7 days",
        help_text="Use only on your own device. Leave unchecked for a 4-hour login.",
    )


class InvitationRegistrationForm(forms.Form):
    token=forms.RegexField(regex=r'^[0-9a-f]{64}$',widget=forms.HiddenInput)
    username=forms.RegexField(regex=r'^[a-z][a-z0-9_-]{2,31}$',max_length=32,help_text='3-32 lowercase letters, digits, underscore or hyphen; start with a letter.',widget=forms.TextInput(attrs={'autocomplete':'username'}))
    password1=forms.CharField(label='Password',strip=False,help_text='At least 2 characters. No other password requirements.',widget=forms.PasswordInput(attrs={'autocomplete':'new-password'}))
    password2=forms.CharField(label='Repeat password',strip=False,widget=forms.PasswordInput(attrs={'autocomplete':'new-password'}))
    def clean_username(self):
        from django.contrib.auth.models import User
        name=self.cleaned_data['username']
        if User.objects.filter(username__iexact=name).exists():raise forms.ValidationError('That username is unavailable.')
        return name
    def clean(self):
        from django.contrib.auth.models import User
        from django.contrib.auth.password_validation import validate_password
        data=super().clean()
        if data.get('password1')!=data.get('password2'):self.add_error('password2','Passwords differ.')
        if data.get('password1') and data.get('username'):
            validate_password(data['password1'],User(username=data['username']))
        return data
