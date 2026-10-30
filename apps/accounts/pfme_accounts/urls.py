from django.urls import path
from django.contrib.auth.views import LogoutView
from .views import PlayerLogin,PlayerPassword,page,account_css,presence_view
from .registration import register,registration_script
urlpatterns=[path('accounts/register/',register),path('accounts/register.js',registration_script),path('accounts/presence/',presence_view),path('accounts/login/',PlayerLogin.as_view()),path('accounts/logout/',LogoutView.as_view()),
 path('accounts/password/',PlayerPassword.as_view()),path('account.css',account_css),path('',page),
 path('play',page),path('play.js',page),path('play.css',page),path('audio.js',page)]
