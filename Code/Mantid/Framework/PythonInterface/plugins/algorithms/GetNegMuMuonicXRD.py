from mantid.api import * # PythonAlgorithm, registerAlgorithm, WorkspaceProperty
from mantid.kernel import *

#pylint: disable=no-init
class GetNegMuMuonicXRD(PythonAlgorithm):
    #Dictionary of <element>:<peaks> easily extendible by user.
    muonic_xr ={'Au' :[8135.2,8090.6,8105.4,8069.4,5764.89,5594.97,3360.2,
                       3206.8,2474.22,2341.21,2304.44,1436.05,1391.58,1104.9,
                       899.14,869.98,405.654,400.143],
               'Ag': [3184.7,3147.7,901.2,869.2,308.428,304.759],
               'Cu' :[1512.78,1506.61,334.8,330.26],
               'Zn' :[1600.15,1592.97,360.75,354.29],
               'Pb' :[8523.3,8442.11,5966.0,5780.1,2641.8,2499.7,
                        2459.7,1511.63,1214.12,1028.83,972.3,938.4,
                        437.687,431.285],
               'As' : [1866.9,1855.8,436.6,427.5],
               'Sn' : [3457.3,3412.8,1022.6,982.5,349.953,345.226]}

    def PyInit(self):
        self.declareProperty(StringArrayProperty("Elements", values=[],
                             direction=Direction.Input
                             ))
        self.declareProperty(name="YAxisPosition",
                                    defaultValue=-0.001,
                                    doc="Position for Markers on the y-axis")

    def get_muonic_xr(self, element):
        #retrieve peak values from dictionary Muonic_XR
        peak_values = self.muonic_xr[element]
        return peak_values

    def create_muonic_xr_ws(self, element, y_pos):
        #retrieve the values from Muonic_XR
        xr_peak_values = self.get_muonic_xr(element)
        #Calibrate y-axis for workspace
        y_pos_ws = [y_pos]*len(xr_peak_values)
        xvalues = xr_peak_values
        muon_xr_ws = CreateWorkspace(xvalues, y_pos_ws[:])
        RenameWorkspaces(muon_xr_ws, WorkspaceNames="MuonXRWorkspace_"+element)
        return muon_xr_ws

    def category(self):
        return "Muon;PythonAlgorithms"

    def PyExec(self):
        elements = self.getProperty("Elements").value
        y_position = self.getProperty("YAxisPosition").value
        workspace_list = [None]*len(elements)
        for idx,element in enumerate(elements):
            curr_workspace = self.create_muonic_xr_ws(element, y_position)
            workspace_list[idx] = curr_workspace

        if len(elements) == 1:
            muonic_xr_workspace = workspace_list[0]
            self.log().information(str("Created: "+muonic_xr_workspace.name()))
        else:
            muonic_xr_group = GroupWorkspaces(workspace_list)
            self.log().information(str("Created Group: "+muonic_xr_group.name()))

AlgorithmFactory.subscribe(GetNegMuMuonicXRD)
