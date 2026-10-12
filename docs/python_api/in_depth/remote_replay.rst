Remote Replay
=============

ReconDuck supports remotely replaying a capture, with display and UI interaction happening locally. When using ReconDuck's scripting through the UI, remote replay is generally invisible to the script. The user will select a given host to replay on and as far as any script calls are concerned it is available with all functionality as-if it were open locally.

If you are running purely from a script without the UI to help, you will need to handle making the remote server connection yourself.

Remote Servers
--------------

ReconDuck's remote replay works via RPC over a socket to an instance of ReconDuck running what is terms a remote server. Any python script can launch and listen as a remote server by calling :func:`~reconduck.BecomeRemoteServer`, as well as the Android version of ReconDuck functioning as a remote server by default. Launching a remote server is not detailed in this guide and it is assumed that you have one running and know its hostname.

Connecting to a remote server is done with :func:`~reconduck.CreateRemoteServerConnection`, which returns a :class:`~reconduck.RemoteServer` if the connection was successful. From the remote server connection you can then both launch applications to create new captures as well as replaying capture files.

To launch a program for capture you can use :meth:`~reconduck.RemoteServer.ExecuteAndInject`. This function is analogous to :func:`~reconduck.ExecuteAndInject` detailed in :doc:`launching_programs` except it happens on the remote host.

Limited remote browsing is possible using :meth:`~reconduck.RemoteServer.GetHomeFolder` and :meth:`~reconduck.RemoteServer.ListFolder` to allow users to browse for remote executables for launch. Note that on some platforms this may not list a literal filesystem but a virtualised list of available applications. It is expected that the results will be compatible with the executable needed in :meth:`~reconduck.RemoteServer.ExecuteAndInject`.

When disconnecting from a remote server there are two options - :meth:`~reconduck.RemoteServer.ShutdownConnection` and :meth:`~reconduck.RemoteServer.ShutdownServerAndConnection`. The former only closes the connection and leaves the remote server running. The latter asks the remote server to shut its process down and then closes the connection. When a remote server closes a connection it will delete any temporary captures that it owns which can be specified using :meth:`~reconduck.RemoteServer.TakeOwnershipCapture`.

You are expected to keep the remote server connection alive with :meth:`~reconduck.RemoteServer.Ping` when not actively performing any other commands. If you do not, the connection may time out due to inactivity.

Capture transfer
----------------

Captures can only be opened by the remote server if they exist on disk on the server. Captures can be transferred in both directions using the remote server connection - copied from the server using :meth:`~reconduck.RemoteServer.CopyCaptureFromRemote` and copied to the server using :meth:`~reconduck.RemoteServer.CopyCaptureToRemote`.

Copying a capture to the server does not specify a target filename. The server itself decides where to store the file and returns the pathname from the function. This file on the replay server is owned by it as a temporary capture and will be deleted when the connection is closed.

API selection
-------------

When connected to a remote host you can query what APIs it supports for replay via :meth:`~reconduck.RemoteServer.RemoteSupportedReplays`. This can be used to determine whether or not a capture is capable of being replayed, or possibly for selection of a remote host among several options.

For the remote replay, ReconDuck must use an API locally in a limited fashion to display textures and render meshes. This is completely independent of the API used in the capture and only requires minimal functionality. You can enumerate the available APIs for local proxying using :meth:`~reconduck.RemoteServer.LocalProxies`.

Opening a capture
-----------------

To open a capture for replay you use :meth:`~reconduck.RemoteServer.OpenCapture`. This is analogous to :meth:`~reconduck.CaptureFile.OpenCapture` and returns the same tuple including a :class:`~reconduck.ReplayController` if successful. When opening a capture you can specify a local proxy API using an index in the returned list from :meth:`~reconduck.RemoteServer.LocalProxies`. If you have no preference (this is recommended as it usually does not matter) you can pass ``-1``.