Bodyfit client Unreal Engine plugin.

![](http://knma.github.io/static/bodyfit_ue_plugin.png)

*BodyfitHandler* class and a set of auxiliary structures allow to send body processing requests to Bodyfit server and receive results.

To check if the server is alive, go to its public URL in browser. It should say "Bodyfit is running".

*BodyfitHandler::ProcessBody* function takes necessary values from:
* *BodyfitHandler::URL*: Bodyfit server host
* *BodyfitHandler::Gender*: person's gender
* *BodyfitHandler::FrontImg*: local image path (front view)
* *BodyfitHandler::SideImg*: local image path (left side view)
* *BodyfitHandler::Height*: person's height
and makes a POST request to Bodyfit server.

If request is successful, the server starts body processing. The client then starts the polling with *BodyfitHandler::GetResult* request.

If the result is ready, the server sends json containing the following:
* *linear_measurements*: spine, hands and legs length
* *joints*: locations of 24 SMPL model joints in global space. Please check ```smpl_layer.py``` for the joints order.
* *pose*: corresponding joint rotations in axis-angle representaion.
* *shape*: values of 10 SMPL model shape PCA coefficients
* *loops*: loop measurements (hips, waist, bust, shoulders, hands, legs).
    * *step_point*: the center of the loop in global space
    * *direction*: normalized direction vector of the loop
    * *edge_points*: coordinates of line segment pairs in 2d space (projected onto the plane defined by *step_point* and *direction*).
    * *leftmost point*: coordinates of a leftmost point of the loop in global space
    * *rightmost point*: coordinates of a rightmost point of the loop in global space
    * *length*: length of the loop
* *video*: preview video url
* *frames*: list of preview frame urls
* *model*: url for the final body mesh (.obj)

*BodyfitHandler* transforms results to BlueprintType USTRUCT (*FBodyProcessingResult*) and invokes *BodyfitHandler::ResultReceived* event. If timed out, *BodyfitHandler::ResultPollingTimedOut* is called.
