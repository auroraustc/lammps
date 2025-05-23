.. index:: fix imd

fix imd command
===============

Syntax
""""""

.. code-block:: LAMMPS

   fix ID group-ID imd trate port keyword values ...

* ID, group-ID are documented in :doc:`fix <fix>` command
* imd = style name of this fix command
* port = port number on which the fix listens for an IMD client
* keyword = *unwrap* or *fscale* or *trate* or *nowait*

  .. parsed-literal::

       *unwrap* arg = *on* or *off*
         off = coordinates are wrapped back into the principal unit cell (default)
         on = "unwrapped" coordinates using the image flags used
       *fscale* arg = factor
         factor = floating point number to scale IMD forces (default: 1.0)
       *trate* arg = transmission rate of coordinate data sets (default: 1)
       *nowait* arg = *on* or *off*
         off = LAMMPS waits to be connected to an IMD client before continuing (default)
         on = LAMMPS listens for an IMD client, but continues with the run
       *version* arg = *2* or *3*
         2 = use IMD protocol version 2 (default)
         3 = use IMD protocol version 3.

  The following keywords are only supported for IMD protocol version 3.

  .. parsed-literal::

       *time* arg = *on* or *off*
          off = simulation time is not transmitted (default)
          on = simulation time is transmitted.
       *box* arg = *on* or *off*
          off = simulation box data is not transmitted (default)
          on = simulation box data is transmitted.
       *coordinates* arg = *on* or *off*
          off = atomic coordinates are not transmitted (default)
          on = atomic coordinates are transmitted.
       *velocities* arg = *on* or *off*
          off = atomic velocities are not transmitted (default)
          on = atomic velocities are transmitted.
       *forces* arg = *on* or *off*
          off = atomic forces are not transmitted (default)
          on = atomic forces are transmitted.

Examples
""""""""

.. code-block:: LAMMPS

   fix vmd all imd 5678
   fix comm all imd 8888 trate 5 unwrap on fscale 10.0

Description
"""""""""""

This fix implements the "Interactive MD" (IMD) protocol which allows
realtime visualization and manipulation of MD simulations through the
IMD protocol, as initially implemented in VMD and NAMD.  Specifically it
allows LAMMPS to connect an IMD client, for example the `VMD
visualization program <VMD_>`_ (currently only supports IMDv2) or the
`Python IMDClient <IMDClient_>`_ (supports both IMDv2 and IMDv3), so
that it can monitor the progress of the simulation and interactively
apply forces to selected atoms.

If LAMMPS is compiled with the pre-processor flag
:ref:`-DLAMMPS_ASYNC_IMD <misc>` then fix imd will use POSIX threads to
spawn an IMD communication thread on MPI rank 0 in order to offload data
exchange with the IMD client from the main execution thread and
potentially lower the inferred latencies for slow communication
links. This feature has only been tested under linux.

The source code for this fix includes code developed by the Theoretical
and Computational Biophysics Group in the Beckman Institute for Advanced
Science and Technology at the University of Illinois at
Urbana-Champaign.  We thank them for providing a software interface that
allows codes like LAMMPS to hook to `VMD <VMD_>`_.

Upon initialization of the fix, it will open a communication port on
the node with MPI task 0 and wait for an incoming connection.  As soon
as an IMD client is connected, the simulation will continue and the
fix will send the current coordinates of the fix's group to the IMD
client at every trate MD step. When using r-RESPA, trate applies to
the steps of the outmost RESPA level.  During a run with an active IMD
connection also the IMD client can request to apply forces to selected
atoms of the fix group.

The port number selected must be an available network port number.  On
many machines, port numbers < 1024 are reserved for accounts with
system manager privilege and specific applications. If multiple imd
fixes would be active at the same time, each needs to use a different
port number.

The *nowait* keyword controls the behavior of the fix when no IMD
client is connected. With the default setting of *off*, LAMMPS will
wait until a connection is made before continuing with the
execution. Setting *nowait* to *on* will have the LAMMPS code be ready
to connect to a client, but continue with the simulation. This can for
example be used to monitor the progress of an ongoing calculation
without the need to be permanently connected or having to download a
trajectory file.

The *trate* keyword allows to select how often the coordinate data is
sent to the IMD client. It can also be changed on request of the IMD
client through an IMD protocol message.  The *unwrap* keyword allows
to send "unwrapped" coordinates to the IMD client that undo the
wrapping back of coordinates into the principle unit cell, as done by
default in LAMMPS.  The *fscale* keyword allows to apply a scaling
factor to forces transmitted by the IMD client. The IMD protocols
stipulates that forces are transferred in kcal/mol/Angstrom under the
assumption that coordinates are given in Angstrom. For LAMMPS runs
with different units or as a measure to tweak the forces generated by
the manipulation of the IMD client, this option allows to make
adjustments.

.. versionadded:: 4Feb2025

In `IMDv3 <IMDv3_>`_, the IMD protocol has been extended to allow for
the transmission of simulation time, box dimensions, atomic coordinates,
velocities, and forces. The *version* keyword allows to select the
version of the protocol to be used. The *time*, *box*, *coordinates*,
*velocities*, and *forces* keywords allow to select which data is
transmitted to the IMD client. The default is to transmit all data.

To connect VMD to a listening LAMMPS simulation on the same machine
with fix imd enabled, one needs to start VMD and load a coordinate or
topology file that matches the fix group.  When the VMD command
prompts appears, one types the command:

.. parsed-literal::

   imd connect localhost 5678

This assumes that *fix imd* was started with 5678 as a port
number for the IMD protocol.

The steps to do interactive manipulation of a running simulation in
VMD are the following:

In the Mouse menu of the VMD Main window, select "Mouse -> Force ->
Atom".  You may alternately select "Residue", or "Fragment" to apply
forces to whole residues or fragments. Your mouse can now be used to
apply forces to your simulation. Click on an atom, residue, or fragment
and drag to apply a force. Click quickly without moving the mouse to
turn the force off. You can also use a variety of 3D position trackers
to apply forces to your simulation. Game controllers or haptic devices
with force-feedback such as the Novint Falcon or Sensable PHANTOM allow
you to feel the resistance due to inertia or interactions with neighbors
that the atoms experience you are trying to move, as if they were real
objects. See the `VMD IMD Homepage <imdvmd_>`_ for more details.

If IMD control messages are received, a line of text describing the
message and its effect will be printed to the LAMMPS output screen, if
screen output is active.

.. _VMD: https://www.ks.uiuc.edu/Research/vmd

.. _imdvmd: https://www.ks.uiuc.edu/Research/vmd/imd/

.. _IMDClient: https://github.com/Becksteinlab/imdclient/tree/main/imdclient

.. _IMDv3: https://imdclient.readthedocs.io/en/latest/protocol_v3.html

Restart, fix_modify, output, run start/stop, minimize info
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

No information about this fix is written to :doc:`binary restart files
<restart>`.  None of the :doc:`fix_modify <fix_modify>` options are
relevant to this fix.  No global scalar or vector or per-atom quantities
are stored by this fix for access by various :doc:`output commands
<Howto_output>`.  No parameter of this fix can be used with the
*start/stop* keywords of the :doc:`run <run>` command.  This fix is not
invoked during :doc:`energy minimization <minimize>`.

Restrictions
""""""""""""

This fix is part of the MISC package.  It is only enabled if LAMMPS was
built with that package.  See the :doc:`Build package <Build_package>`
page for more info.

When used in combination with VMD, a topology or coordinate file has to
be loaded, which matches (in number and ordering of atoms) the group the
fix is applied to.  The fix internally sorts atom IDs by ascending
integer value; in VMD (and thus the IMD protocol) those will be assigned
0-based consecutive index numbers.

When using multiple active IMD connections at the same time, each
fix instance needs to use a different port number.

Related commands
""""""""""""""""

none


Default
"""""""

none
